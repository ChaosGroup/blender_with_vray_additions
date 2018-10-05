#include "vfb_thread_manager.h"
#include "cgr_config.h" // PRINT_INFO_EX

#include "vfb_util_defines.h"

using namespace VRayForBlender;
using namespace std;

ThreadManager::ThreadManager(int thCount)
	: m_stop(false)
{
	setThreadCount(thCount);
}

ThreadManager::Ptr ThreadManager::make(int thCount) {
	return ThreadManager::Ptr(new ThreadManager(thCount));
}

ThreadManager::~ThreadManager() {
	// m_workers is not protected because there is no sane way to do this from inside the ThreadManager
	// it can't handle calling some methond and dtor concurrently
	if (!m_workers.empty()) {
		VFB_Assert(!"VFB ThreadManager exiting while threads are running!");
		stop();
	}
}

void ThreadManager::setThreadCount(int count) {
	// it is easier to stop and re-create all threads
	// it does not force us to stop only indivudual threads and syncronize m_stop
	if (count != m_workers.size()) {
		stop();
		m_stop = false;
	}

	if (count > 0) {
		for (int c = 0; c < count; ++c) {
			m_workers.emplace_back(thread(&ThreadManager::workerRun, this, c));
		}
	}
}

void ThreadManager::stop() {
	m_stop = true;

	if (!m_workers.empty()) {
		m_queueCondVar.notify_all();

		for (int c = 0; c < m_workers.size(); ++c) {
			if (m_workers[c].joinable()) {
				m_workers[c].join();
			} else {
				VFB_Assert(!"VFB ThreadManager's thread is not joinable during stop!");
			}
		}

		m_workers.clear();
	}
}

void ThreadManager::addTask(ThreadManager::Task task, ThreadManager::Priority priority) {
	if (m_workers.empty()) {
		// no workers - do the job ourselves
		task(-1, m_stop);
	} else {
		{
			lock_guard<mutex> lock(m_queueMtx);
			if (priority == Priority::HIGH) {
				m_tasks.push_front(task);
			} else {
				m_tasks.push_back(task);
			}
		}
		m_queueCondVar.notify_one();
	}
}

void ThreadManager::workerRun(int thIdx) {
	PRINT_INFO_EX("Thread [%d] starting ...", thIdx);

	while (!m_stop) {
		Task task;
		{
			unique_lock<mutex> lock(m_queueMtx);
			// wait for task or stop
			m_queueCondVar.wait(lock, [this] { return !m_tasks.empty() || m_stop; });

			if (m_stop) {
				break;
			}

			task = m_tasks.front();
			m_tasks.pop_front();
		}

		task(thIdx, m_stop);
	}

	PRINT_INFO_EX("Thread [%d] stopping.", thIdx);
}