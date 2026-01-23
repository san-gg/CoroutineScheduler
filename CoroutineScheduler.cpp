#include <charconv>
#include <format>
#include <sstream>
#include <unordered_set>
#include <cstring>
#include "CoroutineScheduler.hpp"

using namespace CoroutineScheduler;

std::unique_ptr<Runtime> Runtime::instance = std::make_unique<Runtime>();
thread_local CoroutineContext* const coroutineContext = new CoroutineContext();

Runtime::Runtime() {
	this->threadCount = std::thread::hardware_concurrency();
	const char* env = std::getenv("COMAXPROCS");
	if (env != nullptr) {
		auto [ptr, ec] = std::from_chars(env, env + std::strlen(env), this->threadCount);
		if (ec != std::errc{}) {
			throw std::runtime_error("Failed to parse COMAXPROCS environment variable");
		}
	}
	this->workerThreads.reserve(this->threadCount);
	auto proc = std::make_unique<Proc>();
	this->workerThreads.emplace_back(
		std::make_pair(
			std::move(std::thread(&Proc::ThreadMainLoop, proc.get())),
			std::move(proc)
		)
	);
}

Runtime::~Runtime() {
	for (auto& t : this->workerThreads) {
		t.second->ForceExitProc();
	}
	{
		std::lock_guard lock(this->queueMutex);
		for (int i = 0; i < this->workerThreads.size(); i++) {
			this->globalQueue.push_back(nullptr);
		}
	}
	for (auto& t : this->workerThreads) {
		cv.notify_one();
	}
	for (auto& t : this->workerThreads) {
		t.first.join();
	}
}

void CoroutineScheduler::Runtime::EnsureThreadCount()
{
	std::lock_guard lock(this->queueMutex);
	if (this->workerThreads.size() >= this->threadCount)
		return;
	auto proc = std::make_unique<Proc>();
	this->workerThreads.emplace_back(
		std::make_pair(
			std::move(std::thread(&Proc::ThreadMainLoop, proc.get())),
			std::move(proc)
		)
	);
}

void CoroutineScheduler::Runtime::AddTask(ITask* const task) {
	if (task == nullptr || task->state == TaskState::TaskRunning)
		return;
	else if (task->state == TaskState::TaskNotStarted)
		EnsureThreadCount();
	else if (task->state == TaskState::TaskPaused)
		task->state = TaskState::TaskRunning;

	std::lock_guard lock(this->queueMutex);
	this->globalQueue.emplace_back(task);
	cv.notify_one();
}

ITask* CoroutineScheduler::Runtime::FetchTaskFromGlobalQueue()
{
	std::unique_lock lock(this->queueMutex);
	this->cv.wait(lock, [this]() { return !this->globalQueue.empty(); });
	ITask* task = this->globalQueue.front();
	this->globalQueue.pop_front();
	return task;
}

ITask* CoroutineScheduler::Runtime::GetCurrentContextTask()
{
	if (coroutineContext->task != nullptr) {
		return coroutineContext->task;
	}
	return nullptr;
}

void CoroutineScheduler::Runtime::PreemptCurrentTask()
{
	if (coroutineContext->currentProc != nullptr) {
		coroutineContext->task->state = TaskState::TaskPaused;
		//std::cout << std::format("[INFO] Preempting task {}\n", coroutineContext->task->GetTaskName());
		Fiber::SwitchToFiber(coroutineContext->task->fiberHandle, coroutineContext->currentProc->threadHandle);
	}
}

void CoroutineScheduler::Runtime::PreemptForDependentTask(ITask& task)
{
	if (coroutineContext->currentProc != nullptr && task.SetDependentTask(coroutineContext->task)) {
		coroutineContext->task->state = TaskState::TaskPaused;
		//std::cout << std::format("[INFO] Preempting task {}\n", coroutineContext->task->GetTaskName());
		Fiber::SwitchToFiber(coroutineContext->task->fiberHandle, coroutineContext->currentProc->threadHandle);
	}
}

inline Runtime& CoroutineScheduler::Runtime::GetInstance() {
	return *instance;
}

constexpr auto COROUTINE_STACK_SIZE = 8 * 1024;
static void FiberMain(void* args);

void CoroutineScheduler::Proc::ForceExitProc() {
	std::lock_guard<std::mutex> lock(exitmtx);
	forceExit = true;
}

bool CoroutineScheduler::Proc::ShouldExit() {
	std::lock_guard<std::mutex> lock(exitmtx);
	return forceExit;
}

void CoroutineScheduler::Proc::RunTask(ITask* task, std::string& osThreadId)
{
	if (task->state == TaskState::TaskNotStarted) {
		task->fiberHandle = Fiber::CreateFiber(COROUTINE_STACK_SIZE, FiberMain);
	}

	coroutineContext->currentProc = this;
	coroutineContext->task = task;

	Fiber::SwitchToFiber(this->threadHandle, task->fiberHandle);

	switch (task->state) {
	case TaskState::TaskCompleted:
		std::cout << std::format("[INFO] Task {} completed on thread {}\n", task->GetTaskName(), osThreadId);
		if (task->dependentTask != nullptr) {
			RunTask(task->dependentTask, osThreadId);
		}
		if (!task->MarkForDeletion()) {
			delete task;
		}
		break;
	case TaskState::TaskPaused:
		std::cout << std::format("[INFO] Task {} paused on thread {}\n", task->GetTaskName(), osThreadId);
		break;
	};
}

void CoroutineScheduler::Proc::ThreadMainLoop() {
	threadHandle = Fiber::CreateFiberFromThread();
	std::thread::id tid = std::this_thread::get_id();
	std::stringstream _ss;
	_ss << tid;
	auto osThreadId = _ss.str();
	std::cout << std::format("[INFO] Thread {} started.\n", osThreadId);
	while (!ShouldExit()) {
		auto task = Runtime::GetInstance().FetchTaskFromGlobalQueue();
		if (task != nullptr) RunTask(task, osThreadId);
	}
	std::cout << std::format("[INFO] Thread {} Exited.\n", _ss.str());
	Fiber::DeleteFiber(this->threadHandle);
	this->threadHandle = nullptr;
	delete coroutineContext;
}

static void FiberMain(void* args) {
	std::thread::id tid = std::this_thread::get_id();
	std::stringstream _ss;
	_ss << tid;
	std::cout << std::format("[INFO] Executing task: {} on thread {}\n", coroutineContext->task->GetTaskName(), _ss.str());
	coroutineContext->task->state = TaskState::TaskRunning;

	coroutineContext->task->Execute();

	coroutineContext->task->state = TaskState::TaskCompleted;
	Fiber::SwitchToFiber(coroutineContext->task->fiberHandle, coroutineContext->currentProc->threadHandle);
}
