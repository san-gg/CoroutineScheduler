#pragma once

#include <deque>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>
#include <memory>
#include <unordered_map>
#include <string>

#include "Task.hpp"

namespace CoroutineScheduler {

	struct CoroutineContext;

	struct Proc {
		std::mutex exitmtx;
		std::condition_variable cv;
		Fiber::FiberHandle threadHandle;
		bool forceExit;

		Proc() : forceExit(false), threadHandle(nullptr) {}
		void ForceExitProc();
		bool ShouldExit();
		void RunTask(ITask* task, std::string& osThreadId);
		void ThreadMainLoop();
	};

	class Runtime {
	private:
		unsigned int threadCount;
		std::vector<std::pair<std::thread, std::unique_ptr<Proc>>> workerThreads;
		std::deque<ITask*> globalQueue;
		std::mutex queueMutex;
		std::condition_variable cv;
		static std::unique_ptr<Runtime> instance;
	public:
		Runtime();
		~Runtime();
		void EnsureThreadCount();
		void AddTask(ITask* task);
		ITask* FetchTaskFromGlobalQueue();

		ITask* GetCurrentContextTask();
		void PreemptCurrentTask();
		void PreemptForDependentTask(ITask& task);

		static Runtime& GetInstance();
	};

	struct CoroutineContext {
		Proc* currentProc = nullptr;
		ITask* task = nullptr;
	};
}
