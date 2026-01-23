#include <iostream>
#include <chrono>

#include "CoroutineScheduler.hpp"
#include "Syscalls.hpp"

namespace CoroutineScheduler
{
namespace Syscall
{
	//-------------------------------------------------------------
	std::unique_ptr<Sleep> Sleep::sleepSyscall = std::make_unique<Sleep>();
	//-------------------------------------------------------------

	//----------------------- Sleep Syscall -----------------------
	Sleep::Sleep() : sleepThread(&Sleep::SleepLoop, this), exit(false) { }
	Sleep::~Sleep() {
		{
			std::lock_guard lock(this->mtx);
			this->exit = true;
		}
		this->cv.notify_one();
		this->sleepThread.join();
	}
	void Sleep::SleepLoop()
	{
		while (true) {
			std::unique_lock lock(this->mtx);
			if (this->sleepTimeQueue.empty())
				this->cv.wait(lock, [this] { return !sleepTimeQueue.empty() || this->exit; });
			else {
				auto nextWakeTime = this->sleepTimeQueue.top();
				this->cv.wait_until(lock, nextWakeTime.first, [this, nextWakeTime] {
					return !this->sleepTimeQueue.empty() && this->sleepTimeQueue.top().first < nextWakeTime.first;
					});
			}
			if (this->exit) break;
			auto now = Clock::now();
			while (!this->sleepTimeQueue.empty() && now >= this->sleepTimeQueue.top().first) {
				auto taskToExecute = this->sleepTimeQueue.top();
				this->sleepTimeQueue.pop();
				Runtime::GetInstance().AddTask(taskToExecute.second);
				now = Clock::now();
			}
		}
	}
	void Sleep::AddSleep(int milliSec, ITask* task)
	{
		std::lock_guard lock(this->mtx);
		this->sleepTimeQueue.push({ Clock::now() + std::chrono::milliseconds(milliSec), task });
		this->cv.notify_one();
	}
	//-------------------------------------------------------------

}
}