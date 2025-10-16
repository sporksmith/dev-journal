#include <stdio.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

int main() {
	// Open an output file in the parent, which the child will inherit.
	int fd = open("output.txt", O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR);
	if (fd == -1) {
		perror("open");
		exit(1);
	}

	pid_t child = fork();
	if (child == 0) {
		// We're in the child. First let's demonstrate that we can
		// slightly-decrease RLIMIT_FSIZE ourselves and still write to
		// the output file, as expected.
		struct rlimit lim;
		if (getrlimit(RLIMIT_FSIZE, &lim) == -1) {
			perror("child getrlimit");
			exit(1);
		}
		printf("child current limit via getrlimit: %lu\n", lim.rlim_cur);
		--lim.rlim_cur;
		if (setrlimit(RLIMIT_FSIZE, &lim) == -1) {
			perror("child setrlimit");
			exit(1);
		}
		if (write(fd, "x", 1) == -1) {
			perror("child write");
			exit(1);
		}
		printf("child wrote successfully\n");

		// wait until parent has had a chance to call prlimit.
		sleep(2);

		// try writing a single byte.
		if (write(fd, "x", 1) == -1) {
			perror("child write");
			exit(1);
		}
		printf("child wrote successfully again\n");

		exit(0);
	} else if (child == -1) {
		perror("fork");
		exit(1);
	}

	// Let child do its initial rlimit twiddling and write.
	sleep(1);

	// Decrement child limit via prlimit.
	struct rlimit rlim;
	if (prlimit(child, RLIMIT_FSIZE, NULL, &rlim) == -1) {
		perror("prlimit (get)");
		exit(1);
	}
	printf("child current limit: %lu\n", rlim.rlim_cur);
	--rlim.rlim_cur;
	if (prlimit(child, RLIMIT_FSIZE, &rlim, NULL) == -1) {
		perror("prlimit (set)");
		exit(1);
	}

	// Find out how the child exited.  Expected behavior is to exit with
	// status 0, but instead it ends up getting SIGXFSZ, as if it had
	// exceeded its RLIMIT_FSIZE.
	int wstatus=0;
	pid_t waitee = waitpid(child, &wstatus, 0);
	if (waitee == -1) {
		perror("waitpid");
		exit(1);
	}

	if (WIFEXITED(wstatus)) {
		printf("child exited with status %d\n", WEXITSTATUS(wstatus));
	} else if (WIFSIGNALED(wstatus)) {
		printf("child killed by signal %d\n", WTERMSIG(wstatus));
	} else {
		printf("unhandled child status");
		exit(1);
	}
}
