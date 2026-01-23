#include <queue>
#include <mutex>
#include <condition_variable>

#include "Task.hpp"

#define SIMPLE_CHANNEL_COMPLEX
// #define SIMPLE_CHANNEL_SIMPLE

namespace CoroutineScheduler
{
namespace Channel
{
	constexpr std::chrono::milliseconds ChannelStdWait = std::chrono::milliseconds(500);

#ifdef SIMPLE_CHANNEL_COMPLEX

	template<typename T>
	class SimpleChannel {
		std::queue<T> _value;
		unsigned int size;
		std::queue<ITask*> senderPreemptedTask, receiverPreemptedTask;

		bool receiverNotified = false;
		std::mutex value_mtx, receiver_mtx;
		std::condition_variable value_cv, receiver_cv;
	public:

		SimpleChannel() : size(1) {}
		SimpleChannel(unsigned int sz) : size(sz) {}

		void Send(T value) {
			auto& runtime = Runtime::GetInstance();
			while (true) {
				std::unique_lock lock(this->value_mtx);
				if (this->value_cv.wait_for(lock, ChannelStdWait, [this] { return this->_value.size() < this->size; })) {
					this->_value.push(value);
					break;
				}
				else {
					this->senderPreemptedTask.push(runtime.GetCurrentContextTask());
					lock.unlock();
					this->notifyReceiver();
					runtime.PreemptCurrentTask();
				}
			}
			this->notifyReceiver();
		}

		T Receive() {
			auto& runtime = Runtime::GetInstance();
			while (true) {
				bool preempt = false;
				{
					std::unique_lock lock(this->receiver_mtx);
					preempt = !this->receiver_cv.wait_for(lock, ChannelStdWait, [this] { return this->receiverNotified; });
					if (preempt) this->receiverPreemptedTask.push(runtime.GetCurrentContextTask());
				}
				if (preempt) runtime.PreemptCurrentTask();
				{
					std::lock_guard lock1(this->value_mtx);
					if (this->_value.empty()) {
						std::lock_guard lock2(this->receiver_mtx);
						this->receiverNotified = false;
						this->notifySender();
						continue;
					}
					if (!this->_value.empty()) {
						T value = this->_value.front();
						this->_value.pop();
						this->notifyReceiver();
						return value;
					}
				}
			}
		}

		void notifySender() {
			//std::lock_guard lock(this->value_mtx); // **Note:** already locked in receiver
			this->value_cv.notify_one();
			if (!this->senderPreemptedTask.empty()) {
				auto senderTask = this->senderPreemptedTask.front();
				this->senderPreemptedTask.pop();
				Runtime::GetInstance().AddTask(senderTask);
			}
		}

		void notifyReceiver() {
			std::lock_guard lock(this->receiver_mtx);
			this->receiverNotified = true;
			this->receiver_cv.notify_one();
			if (!this->receiverPreemptedTask.empty()) {
				auto receiverTask = this->receiverPreemptedTask.front();
				this->receiverPreemptedTask.pop();
				Runtime::GetInstance().AddTask(receiverTask);
			}
		}
	};
#elif defined(SIMPLE_CHANNEL_SIMPLE)
	template<typename T>
	class SimpleChannel {
		std::queue<T> _value;
		unsigned int size;
		std::queue<ITask*> senderWaitQueue;
		std::queue<ITask*> receiverWaitQueue;
		std::mutex mtx; // Single mutex for state protection

	public:
		SimpleChannel() : size(1) {}
		SimpleChannel(unsigned int sz) : size(sz) {}

		void Send(T value) {
			auto& runtime = Runtime::GetInstance();

			while (true) {
				std::unique_lock lock(this->mtx);

				// 1. Check Condition
				if (this->_value.size() < this->size) {
					// Success path
					this->_value.push(value);

					// Wake up a blocked receiver if any
					if (!this->receiverWaitQueue.empty()) {
						auto t = this->receiverWaitQueue.front();
						this->receiverWaitQueue.pop();
						runtime.AddTask(t);
					}
					return; // Lock releases here
				}

				// 2. Fail path: Must Wait
				ITask* current = runtime.GetCurrentContextTask();
				this->senderWaitQueue.push(current);

				// CRITICAL: We must ensure we don't race with the waker.
				// Ideally, 'PreemptCurrentTask' should handle the unlocking to be atomic, 
				// but assuming standard implementation, we unlock then yield.
				lock.unlock();

				// 3. Suspend (Yield)
				// Do NOT sleep/wait. Yield control to the scheduler immediately.
				runtime.PreemptCurrentTask();

				// Loop restarts when we are woken up
			}
		}

		T Receive() {
			auto& runtime = Runtime::GetInstance();

			while (true) {
				std::unique_lock lock(this->mtx);

				if (!this->_value.empty()) {
					T val = this->_value.front();
					this->_value.pop();

					// Wake up a blocked sender if any
					if (!this->senderWaitQueue.empty()) {
						auto t = this->senderWaitQueue.front();
						this->senderWaitQueue.pop();
						runtime.AddTask(t);
					}
					return val;
				}

				// Queue empty, we must wait
				this->receiverWaitQueue.push(runtime.GetCurrentContextTask());
				lock.unlock();
				runtime.PreemptCurrentTask();
			}
		}
	};
#endif
}
}