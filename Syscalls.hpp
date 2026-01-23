#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "Task.hpp"

namespace CoroutineScheduler
{
namespace Syscall
{
	class Sleep {
		using Clock = std::chrono::steady_clock;
		using SleepPair = std::pair<Clock::time_point, ITask*>;

		static std::unique_ptr<Sleep> sleepSyscall;

		std::thread sleepThread;
		std::mutex mtx;
		std::condition_variable cv;
		std::priority_queue<SleepPair, std::vector<SleepPair>, std::greater<SleepPair>> sleepTimeQueue;
		bool exit;

		void SleepLoop();
	public:
		Sleep();
		~Sleep();
		void AddSleep(int milliSec, ITask* task);
		inline static Sleep& GetInstance() { return *sleepSyscall; };
	};
}
}
