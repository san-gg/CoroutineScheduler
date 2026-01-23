#pragma once

#include <iostream>
#include <format>
#include <tuple>
#include <utility>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <type_traits>
#include "./Fiber/fiber.h"

namespace CoroutineScheduler {
	enum TaskState {
		TaskNotStarted,
		TaskRunning,
		TaskCompleted,
		TaskPaused
	};
	class ITask {
	public:
		Fiber::FiberHandle fiberHandle;
		TaskState state;
		ITask* dependentTask;

		ITask() : state(TaskState::TaskNotStarted), fiberHandle(nullptr), dependentTask(nullptr) {}
		virtual const char* const GetTaskName() const = 0;
		virtual void Execute() = 0;
		virtual void Await() = 0;
		virtual bool SetDependentTask(ITask* task) = 0;
		virtual bool MarkForDeletion() = 0;
		virtual ~ITask() = default;
	};

	template<typename F, typename... A>
	class Task : public ITask {
	protected:
		const char* const taskName;
		// Use decay_t to store copies of arguments so they don't go out of scope
		std::decay_t<F> function;
		std::tuple<std::decay_t<A>...> arguments;

		std::mutex mtx;
		std::condition_variable cv;

		bool isCompleted = false;
		bool isMarkedForDeletion = false;

	public:
		// Deleted copy/move because mutexes cannot be moved
		Task(const Task&) = delete;
		Task& operator=(const Task&) = delete;

		Task(const char* const taskName, F&& func, A&&... args)
			: taskName(taskName), function(std::forward<F>(func)), arguments(std::forward<A>(args)...) {
		}

		const char* const GetTaskName() const override {
			return taskName;
		}

		void Await() override {
			std::unique_lock<std::mutex> lock(mtx);
			cv.wait(lock, [this]() { return this->isCompleted; });
		}

		bool SetDependentTask(ITask* task) override {
			std::lock_guard<std::mutex> lock(mtx);
			if (isCompleted) {
				return false;
			}
			this->dependentTask = task;
			return true;
		}

		virtual void Execute() override {
			std::apply(function, arguments);
			{
				std::lock_guard<std::mutex> lock(mtx);
				isCompleted = true;
			}
			cv.notify_all();
		}

		virtual bool MarkForDeletion() override {
			std::lock_guard<std::mutex> lock(mtx);
			if (isMarkedForDeletion) return false;
			isMarkedForDeletion = true;
			return true;
		}

		virtual ~Task() {
			std::cout << std::format("[INFO] Cleaning up Coroutine resource {}\n", GetTaskName());
			Fiber::DeleteFiber(fiberHandle);
			this->fiberHandle = nullptr;
			this->dependentTask = nullptr;
		}
	};

	template<typename R, typename F, typename... A>
	class TaskWithReturnValue : public Task<F, A...> {
		std::unique_ptr<R> returnValue;
	public:
		using Task<F, A...>::Task; // Inherit constructor

		void Execute() override {
			std::lock_guard<std::mutex> lock(this->mtx);
			// Use std::apply and store the result
			R ret = std::apply(this->function, this->arguments);
			returnValue = std::make_unique<R>(std::move(ret));
			{
				std::lock_guard<std::mutex> lock(Task<F, A...>::mtx);
				Task<F, A...>::isCompleted = true;
			}
			Task<F, A...>::cv.notify_all();
		}

		R const* GetReturnValue() {
			Task<F, A...>::Await();
			return returnValue.get();
		}
	};
}