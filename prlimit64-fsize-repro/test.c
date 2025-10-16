#include <stdio.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <limits.h>

int main() {
	// Open an output file
	int fd = open("output.txt", O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR);
	if (fd == -1) {
		perror("open");
		exit(1);
	}

	if (write(fd, "x", 1) == -1) {
		perror("write");
		exit(1);
	}
	printf("wrote before changing rlimit\n");

	struct rlimit initial;
	if (getrlimit(RLIMIT_FSIZE, &initial) == -1) {
		perror("getrlimit");
		exit(1);
	}
	printf("current limit unsigned:%lu signed:%ld\n", initial.rlim_cur, initial.rlim_cur);
	if (initial.rlim_cur != RLIM_INFINITY) {
		printf("this repro only works when current limit is RLIM_INFINITY\n");
		exit(1);
	}

	// Setting rlimit to signed max is ok.
	struct rlimit max_unsigned = {.rlim_cur = LONG_MAX, .rlim_max = RLIM_INFINITY};
	if (setrlimit(RLIMIT_FSIZE, &max_unsigned) == -1) {
		perror("setrlimit");
		exit(1);
	}
	if (write(fd, "x", 1) == -1) {
		perror("write");
		exit(1);
	}
	printf("wrote with rlimit=LONG_MAX\n");

	// Decrementing the initial limit from RLIM_INFINITY will cause the next write to fail.
	// I think the problem is that this is being treated as a signed number somewhere,
	// with RLIM_INFINITY (0xffff_ffff_ffff_ffff == -1) special-cased. Decrementing causes
	// it to be interpreted as -2 instead of a large positive number.
	struct rlimit decremented = {.rlim_cur = initial.rlim_cur-1, .rlim_max = initial.rlim_max};
	if (setrlimit(RLIMIT_FSIZE, &decremented) == -1) {
		perror("setrlimit");
		exit(1);
	}
	if (write(fd, "x", 1) == -1) {
		perror("write");
		exit(1);
	}
	printf("wrote with rlimit=(RLIMIT_INFINITY-1)\n");
}
