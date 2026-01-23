#pragma once

#include <memory>
#include "../CoroutineScheduler.hpp"
#include "../Channel.hpp"
#include "../Syscalls.hpp"

namespace Coroutine {

	namespace Syscall
	{
		inline void Sleep(int milliSec) {
			auto& cs = CoroutineScheduler::Runtime::GetInstance();
			auto task = cs.GetCurrentContextTask();
			if (task != nullptr) {
				CoroutineScheduler::Syscall::Sleep::GetInstance().AddSleep(milliSec, task);
				cs.PreemptCurrentTask();
			}
			else std::this_thread::sleep_for(std::chrono::milliseconds(milliSec));
		}
	}

	template<typename T>
	class Channel {
		std::shared_ptr<CoroutineScheduler::Channel::SimpleChannel<T>> chan;
	public:
		Channel() : chan(std::make_shared<CoroutineScheduler::Channel::SimpleChannel<T>>()) {}
		/*void MarkComplete() {
			chan->MarkComplete();
		}
		bool IsCompleted() {
			return chan->IsCompleted();
		}*/
		void Send(T val) {
			chan->Send(val);
		}
		T Receive() {
			return chan->Receive();
		}

		class Sender {
			std::shared_ptr<CoroutineScheduler::Channel::SimpleChannel<T>> chan;
			Sender(std::shared_ptr<CoroutineScheduler::Channel::SimpleChannel<T>> c) : chan(c) {}
		public:
			/*void MarkComplete() {
				chan->MarkComplete();
			}*/
			void Send(T val) {
				chan->Send(val);
			}
			friend class Channel<T>;
		};

		class Receiver {
			std::shared_ptr<CoroutineScheduler::Channel::SimpleChannel<T>> chan;
			Receiver(std::shared_ptr<CoroutineScheduler::Channel::SimpleChannel<T>> c) : chan(c) {}
		public:
			/*bool IsCompleted() {
				return chan->IsCompleted();
			}*/
			T Receive() {
				return chan->Receive();
			}
			friend class Channel<T>;
		};

		Sender* GetSender() {
			return new Sender(this->chan);
		}
		Receiver* GetReceiver() {
			return new Receiver(this->chan);
		}
	};

	template<typename T>
	class BufferedChannel {
		std::shared_ptr <CoroutineScheduler::Channel::SimpleChannel<T>> chan;
	public:
		BufferedChannel(unsigned int bufferSize) : chan(std::make_shared<CoroutineScheduler::Channel::SimpleChannel<T>>(bufferSize)) { }
		void Send(T val) {
			chan->Send(val);
		}
		T Receive() {
			return chan->Receive();
		}

		class Sender {
			std::shared_ptr<CoroutineScheduler::Channel::SimpleChannel<T>> chan;
			Sender(std::shared_ptr<CoroutineScheduler::Channel::SimpleChannel<T>> c) : chan(c) {}
		public:
			void MarkComplete() {
				chan->MarkComplete();
			}
			void Send(T val) {
				chan->Send(val);
			}
			friend class BufferedChannel<T>;
		};

		class Receiver {
			std::shared_ptr<CoroutineScheduler::Channel::SimpleChannel<T>> chan;
			Receiver(std::shared_ptr<CoroutineScheduler::Channel::SimpleChannel<T>> c) : chan(c) {}
		public:
			bool IsCompleted() {
				return chan->IsCompleted();
			}
			T Receive() {
				return chan->Receive();
			}
			friend class BufferedChannel<T>;
		};

		Sender* GetSender() {
			return new Sender(this->chan);
		}
		Receiver* GetReceiver() {
			return new Receiver(this->chan);
		}
	};

	template<typename R, typename F, typename... A>
	class ResultState {
		CoroutineScheduler::ITask* task;
	public:
		ResultState(CoroutineScheduler::ITask* t) : task(t) {}

		~ResultState() {
			CoroutineScheduler::Runtime::GetInstance().PreemptForDependentTask(*task);
			task->Await();
			if (!task->MarkForDeletion()) {
				delete task;
			}
			task = nullptr;
		}

		void Await() {
			CoroutineScheduler::Runtime::GetInstance().PreemptForDependentTask(*task);
			task->Await();
		}

		R GetReturnValue() {
			CoroutineScheduler::TaskWithReturnValue<R, F, A...>* derived = static_cast<CoroutineScheduler::TaskWithReturnValue<R, F, A...>*>(task);
			return *(derived->GetReturnValue());
		}
	};

	template<typename F, typename... A>
	auto Run(const char* const taskName, F&& func, A&&... args) {
		using ReturnType = std::invoke_result_t<F, A...>;

		CoroutineScheduler::ITask* task = nullptr;
		if constexpr (std::is_void_v<ReturnType>)
			task = new CoroutineScheduler::Task<F, A...>(taskName, std::forward<F>(func), std::forward<A>(args)...);
		else
			task = new CoroutineScheduler::TaskWithReturnValue<ReturnType, F, A...>(taskName, std::forward<F>(func), std::forward<A>(args)...);

		CoroutineScheduler::Runtime::GetInstance().AddTask(task);

		return std::move(std::make_shared<ResultState<ReturnType, F, A...>>(task));
	}
}
