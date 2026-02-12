// Measuring thread switching time using a UNIX pipe.
//
// Sanket [https://github.com/san-gg]
// This code is in the public domain.

#include <iostream>
#include <thread>
#include <chrono>
#include <format>

#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

using Clock = std::chrono::steady_clock;

void errExit(const char* s) {
	std::cout << s << std::endl;
	exit(EXIT_FAILURE);
}

struct PipeInfo {
	int readfd;
	int writefd;
};

// This function accepts a pipe information structure. It then runs a loop for
// num_iterations, where in each iteration it:
//   * Reads a byte from pipeinfo.readfd
//   * Writes a byte to pipeinfo.writefd
//
// Each iteration also incurs two context switches - one to the other thread
// (when read() is called) and one back (when it completes successfully).
void ping_pong(struct PipeInfo pipeinfo, int num_iterations) {
	std::thread::id tid = std::this_thread::get_id();
	std::cout << "Thread " << tid << " ping_pong\n";
	std::cout << std::format("  readfd {}; writefd {}\n", pipeinfo.readfd, pipeinfo.writefd);
	
	char buf[2];
	for (int i = 0; i < num_iterations; ++i) {
		if (read(pipeinfo.readfd, buf, 1) != 1) {
			errExit("read");
		}
		if (write(pipeinfo.writefd, buf, 1) != 1) {
			errExit("write");
		}
	}
}

const int NUM_ITERATIONS = 100000;

void* threadfunc(void* p) {
	struct PipeInfo* pipe_info = (struct PipeInfo*)p;
	ping_pong(*pipe_info, NUM_ITERATIONS);
	return NULL;
}

void measure_self_pipe(int num_iterations) {
	int pfd[2];
	char writebuf[2];
	char readbuf[2];
	
	if (pipe(pfd) == -1) {
		errExit("pipe");
	}
	
	// Simple test: write a char to a pipe, test that it arrived as expected.
	writebuf[0] = 'j';
	if (write(pfd[1], writebuf, 1) != 1) {
		errExit("write");
	}
	if (read(pfd[0], readbuf, 1) != 1) {
		errExit("read");
	}
	
	if (readbuf[0] != 'j') {
		printf("Boo, got %c back from the pipe\n", readbuf[0]);
		exit(1);
	}
	
	// Now the timing test: in each loop iteration, write a byte into a pipe and
	// then immediately read it.
	const auto t1 = Clock::now();
	for (int i = 0; i < num_iterations; ++i) {
		if (write(pfd[1], writebuf, 1) != 1) {
			errExit("write");
		}
		if (read(pfd[0], readbuf, 1) != 1) {
			errExit("read");
		}
	}
	const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t1);
	double elapsed_per_itr = elapsed.count() / (double)num_iterations;
	std::cout << "measure_self_pipe: " << elapsed.count() << " us for " << num_iterations << " iterations (" << elapsed_per_itr << " us / iter)\n";
}

int main(int argc, const char** argv) {
	measure_self_pipe(NUM_ITERATIONS);
	
	// Create two pipes, one for sending data from main to child thread; another
	// // for the other direction. Set up the PipeInfo for each side appropriately.
	int main_to_child[2];
	if (pipe(main_to_child) == -1) {
		errExit("pipe");
	}
	int child_to_main[2];
	if (pipe(child_to_main) == -1) {
		errExit("pipe");
	}

	struct PipeInfo main_fds = {
		.readfd = child_to_main[0],
		.writefd = main_to_child[1]
	};
	struct PipeInfo child_fds = {
		.readfd = main_to_child[0],
		.writefd = child_to_main[1]
	};
	
	std::thread childt(threadfunc, (void*)&child_fds);
	
	// For main, seed the ping-pong by writing a word into the write pipe, since
	// the child will wait for it initially.
	
	char buf[2] = {'k'};
	if (write(main_fds.writefd, buf, 1) != 1) {
		errExit("write");
	}
	
	const auto t1 = Clock::now();
	ping_pong(main_fds, NUM_ITERATIONS);
	const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t1);
	
	const int nswitches = NUM_ITERATIONS * 2;
	std::cout << std::format("{} context switches in {} us ({} us / switch)\n", nswitches, elapsed.count(), ((double)elapsed.count() / (double)nswitches));
	
	childt.join();
	
	struct rusage ru;
	if (!getrusage(RUSAGE_SELF, &ru)) {
		printf("From getrusage:\n");
		printf("  voluntary switches = %ld\n", ru.ru_nvcsw);
		printf("  involuntary switches = %ld\n", ru.ru_nivcsw);
	}
	
	return 0;
}
