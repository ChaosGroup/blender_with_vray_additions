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

class CondWaitGroup {
public:
	CondWaitGroup(uint32_t tasks)
		: m_remaining(tasks) {}

	CondWaitGroup(const CondWaitGroup &) = delete;
	CondWaitGroup & operator=(const CondWaitGroup &) = delete;

	void done() {
		std::lock_guard<std::mutex> lock(m_mtx);
		if (--m_remaining == 0) {
			m_condVar.notify_all();
		}
	}

	void wait() {
		std::unique_lock<std::mutex> lock(m_mtx);
		m_condVar.wait(lock, [this]() { return this->m_remaining == 0; });
	}


private:
	uint32_t                m_remaining;
	std::mutex              m_mtx;
	std::condition_variable m_condVar;
};

class BusyWaitGroup {
public:
	BusyWaitGroup(uint32_t tasks)
		: m_remaining(tasks) {}

	BusyWaitGroup(const BusyWaitGroup &) = delete;
	BusyWaitGroup & operator=(const BusyWaitGroup &) = delete;

	void done() {
		--m_remaining;
	}

	void wait(int itervalMs = 10) {
		while (m_remaining > 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(itervalMs));
		}
	}

private:
	std::atomic_uint32_t    m_remaining;
};

class ThreadManager {
public:
	typedef std::shared_ptr<ThreadManager> Ptr;

	// intended to be used with lambda which will capture all neded data
	// must obey stop flag asap, threadIndex == -1 means calling thread (thCount == 0)
	typedef std::function<void(int threadIndex, const volatile bool & stop)> Task;

	enum class Priority {
		LOW, HIGH,
	};

	// create new Thread Manager with @thCount threads running
	// thCount 0 will mean that addTask will block and complete the task on the current thread
	static Ptr make(int thCount);

	ThreadManager(const ThreadManager &) = delete;
	ThreadManager & operator=(const ThreadManager &) = delete;

	// stop must be called before reaching dtor
	~ThreadManager();

	// stops all threads and discards any tasks not yet started
	// if thread count is 0, stop will still set the flag for stop to true
	// and if addTask was called from another thread it will signal the task to stop
	void stop();

	// @task with LOW  @priority will be added at the end      of queue
	// @task with HIGH @priority will be added at the begining of queue
	// okay to be called concurrently
	void addTask(Task task, Priority priority);
private:
	ThreadManager(int thCount);

	void workerRun(int thIdx);

	std::mutex               m_queueMtx;
	std::condition_variable  m_queueCondVar;
	std::deque<Task>         m_tasks;
	std::vector<std::thread> m_workers;
	volatile bool            m_stop;
};

} // namespace VRayForBlender

#endif // VRAY_FOR_BLENDER_THREAD_MANAGER_H
