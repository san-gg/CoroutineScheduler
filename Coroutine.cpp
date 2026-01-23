#include <chrono>
#include <format>
#include "includes/Coroutine.h"

void Func2(Coroutine::Channel<int>::Receiver* recv, Coroutine::Channel<bool>::Sender* done) {
	while (true)
	{
		int v = recv->Receive();
		std::cout << std::format("Func2 received : {}\n", v);
		if (v == -1) break;
		// Coroutine::Syscall::Sleep(1000);
	}
	done->Send(true);
	delete recv;
	delete done;
}

void Func(Coroutine::Channel<int>::Receiver* recv, Coroutine::Channel<bool>::Sender* done) {
	while (true)
	{
		int v = recv->Receive();
		std::cout << std::format("Func received : {}\n", v);
		if (v == -1) break;
		// Coroutine::Syscall::Sleep(1000);
	}
	done->Send(true);
	delete recv;
	delete done;
}

void CoMain(Coroutine::Channel<int>::Sender* sender, Coroutine::Channel<bool>::Receiver* done) {
	for (int i = 0; i < 10; i++) {
		sender->Send(i);
		std::cout << std::format("CoMain Sent {}\n", i);
		Coroutine::Syscall::Sleep(500);
	}
	sender->Send(-1);
	done->Receive();
	sender->Send(-1);
	done->Receive();
	delete sender;
	delete done;
}

void PrintLoop() {
	for (int i = 0; i < 20; i++) {
		std::cout << std::format("PrintLoop iteration {}\n", i);
		Coroutine::Syscall::Sleep(700);
	}
}

int main() {
	Coroutine::Channel<int> chan;
	auto* sender = chan.GetSender();
	auto* receiver = chan.GetReceiver();
	auto* receiver2 = chan.GetReceiver();
	
	Coroutine::Channel<bool> done;
	auto* doneSender = done.GetSender();
	auto* doneSender2 = done.GetSender();
	auto* doneReceiver = done.GetReceiver();

	auto res1 = Coroutine::Run("CoMain", CoMain, sender, doneReceiver);
	auto res2 = Coroutine::Run("Func", Func, receiver, doneSender);
	auto res3 = Coroutine::Run("Func2", Func2, receiver2, doneSender2);

	auto res4 = Coroutine::Run("PrintLoop", PrintLoop);
}
