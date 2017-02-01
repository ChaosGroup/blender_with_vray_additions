/*
 * Copyright (c) 2015, Chaos Software Ltd
 *
 * V-Ray For Blender
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef VRAY_FOR_BLENDER_THREAD_MANAGER_H
#define VRAY_FOR_BLENDER_THREAD_MANAGER_H

#include <functional>
#include "BLI_task.h"

#include <thread>
#include <mutex>
#include <condition_variable>

#include <memory>
#include <deque>
#include <vector>
#include <atomic>
#include <chrono>

namespace VRayForBlender {

/// Class representing thread safe counter with the ability to wait for all tasks to be done
/// Implemented using mutex + cond var
class CondWaitGroup {
public:
	/// Initialize with number of tasks
	CondWaitGroup(int tasks)
		: m_remaining(tasks) {}

	CondWaitGroup(const CondWaitGroup &) = delete;
	CondWaitGroup & operator=(const CondWaitGroup &) = delete;

	/// Mark one task as done (substract 1 from counter)
	void done() {
		std::lock_guard<std::mutex> lock(m_mtx);
		if (--m_remaining == 0) {
			m_condVar.notify_all();
		}
	}

	/// Get number of tasks remaining at call time, could be less when function returns
	int remaining() const {
		return m_remaining;
	}

	/// Block until all tasks are done
	void wait() {
		std::unique_lock<std::mutex> lock(m_mtx);
		m_condVar.wait(lock, [this]() { return this->m_remaining == 0; });
	}


private:
	int                     m_remaining; ///< number of remaining tasks
	std::mutex              m_mtx;       ///< lock protecting m_remaining
	std::condition_variable m_condVar;   ///< cond var to wait on m_remaining
};

/// Class representing thread safe counter with the ability to wait for all tasks to be done
/// Implemented using atomic int + sleep/busy wait
class BusyWaitGroup {
public:
	BusyWaitGroup(int tasks)
		: m_remaining(tasks) {}

	BusyWaitGroup(const BusyWaitGroup &) = delete;
	BusyWaitGroup & operator=(const BusyWaitGroup &) = delete;

	/// Mark one task as done (substract 1 from counter)
	void done() {
		--m_remaining;
	}

	/// Get number of tasks remaining at call time, could be less when function returns
	int remaining() const {
		return m_remaining;
	}

	/// Block until all tasks are done
	/// @intervalMs - if positive will sleep that many ms between checks on the counter
	///               else it will busy wait
	void wait(int intervalMs = 10) {
		while (m_remaining > 0) {
			if (intervalMs > 0) {
				std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
			}
		}
	}

private:
	std::atomic<int> m_remaining; ///< number of remaining tasks
};

/// RAII wrapper over a task made for CondWaitGroup/BusyWaitGroup this will call
/// the .done() method for the provided wait group object in destructor
/// This class ensures that the done method is called for each task in threaded code so we dont
/// block on wait group's wait call indefinitely
template <typename WGType>
class RAIIWaitGroupTask {
public:
	/// Construct with reference to a WaitGroup
	RAIIWaitGroupTask(WGType & waitGroup)
		: m_waitGroup(waitGroup) {}

	/// Call the .done() method for the group
	~RAIIWaitGroupTask() {
		m_waitGroup.done();
	}

	RAIIWaitGroupTask(const RAIIWaitGroupTask &) = delete;
	RAIIWaitGroupTask & operator=(const RAIIWaitGroupTask &) = delete;
private:
	WGType & m_waitGroup;
};

/// Basic thread manager able to execute tasks on different threads
class ThreadManager {
public:
	typedef std::shared_ptr<ThreadManager> Ptr;

	// Intended to be used with lambda which will capture all neded data
	// must obey stop flag asap, threadIndex == -1 means calling thread (thCount == 0)
	typedef std::function<void(int threadIndex, const volatile bool & stop)> Task;

	enum class Priority {
		LOW, HIGH,
	};

	// Create new Thread Manager with @thCount threads running
	// thCount 0 will mean that addTask will block and complete the task on the current thread
	static Ptr make(int thCount);

	ThreadManager(const ThreadManager &) = delete;
	ThreadManager & operator=(const ThreadManager &) = delete;

	// stop must be called before reaching dtor
	~ThreadManager();

	// Get number of worker threads created for the instance
	int workerCount() const {
		return m_workers.size();
	}

	// Stop all threads and discards any tasks not yet started
	// if thread count is 0, stop will still set the flag for stop to true
	// and if addTask was called from another thread it will signal the task to stop
	void stop();

	// Add task to queue
	// @task with LOW  @priority will be added at the end      of queue
	// @task with HIGH @priority will be added at the begining of queue
	// okay to be called concurrently
	void addTask(Task task, Priority priority);
private:
	/// Initialize ThreadManager
	/// @thCount - number of threads to create, if 0 all tasks will be executed immediately on calling thread
	ThreadManager(int thCount);

	/// Base function for each thread
	void workerRun(int thIdx);

	std::mutex               m_queueMtx;     ///< lock guarding @m_tasks
	std::condition_variable  m_queueCondVar; ///< cond var for threads to wait for new tasks
	std::deque<Task>         m_tasks;        ///< queue of the pending tasks
	std::vector<std::thread> m_workers;      ///< all worker threads created for this instace
	volatile bool            m_stop;         ///< if set to true, will stop all threads, also passed to each task as second argument
};

} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_THREAD_MANAGER_H
