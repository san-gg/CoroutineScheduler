// Measuring iterations per sec using a UNIX pipe.
//
// Sanket [https://github.com/san-gg]
// This code is in the public domain.

#include <iostream>
#include <thread>
#include <chrono>
#include <format>
#include <cstring>

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

// The child thread spins in a loop reading a 4-byte message from the read pipe
// and echoing it into the write pipe.
void threadfunc(void* p) {
	struct PipeInfo* pipe_info = (struct PipeInfo*)p;

	char buf[4];
	while(true) {
		int read_rc = read(pipe_info->readfd, buf, 4);
		if (read_rc < 0) {
			errExit("read");
		} else if (read_rc != 4) {
			break;
		}
		if (write(pipe_info->writefd, buf, 4) != 4) {
			errExit("write");
		}
	}

}


int main(int argc, const char** argv) {
	
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
	
	std::thread childth(threadfunc, (void*)&child_fds);
	
	const int NUM_ITERATIONS = 200000;
	const char* msg = "abc";
	char buf[4];
	
	const auto t1 = Clock::now();

	for (int i = 0; i < NUM_ITERATIONS; i++) {
		if (write(main_fds.writefd, msg, 4) != 4) {
			errExit("write");
		}
		if (read(main_fds.readfd, buf, 4) != 4) {
			errExit("read");
		}

		if (strcmp(buf, msg)) {
			std::cout << "Error in comparison\n";
			exit(1);
		}
	}

	const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t1);
	double iters_per_sec = (double)(NUM_ITERATIONS * 1e6) / ((double)elapsed.count());

	std::cout << std::format("{} iterations took {} us. {} iters/sec\n", NUM_ITERATIONS, elapsed.count(), iters_per_sec);
	
	close(main_fds.writefd);
	close(main_fds.readfd);

	childth.join();
	
	return 0;
}
