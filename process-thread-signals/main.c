#include <stdatomic.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

// Atomically incremented by signal handler.
_Atomic int count = 0;

// Signal handler.
void handler(int signo) {
  if(signo != SIGUSR1){
    abort();
  }
  atomic_fetch_add(&count, 1);
}

int main(int argc, char **argv) {
  // Block SIGUSR1
  sigset_t mask;
  if (sigemptyset(&mask) != 0) {
    abort();
  }
  if (sigaddset(&mask, SIGUSR1) != 0) {
    abort();
  }
  if (sigprocmask(SIG_BLOCK, &mask, NULL) != 0) {
    abort();
  }

  // Set handler for SIGUSR1. SA_NODEFER isn't required here, but see below.
  if (sigaction(SIGUSR1, &(struct sigaction){.sa_handler = handler, .sa_flags = SA_NODEFER}, NULL) != 0) {
    abort();
  }

  // Get pid and tid
  pid_t pid = getpid();
  if (pid < 0) {
    abort();
  }
  pid_t tid = syscall(SYS_gettid);
  if (tid < 0) {
    abort();
  }

  // Set signal pending in process. Won't be delivered yet since it's blocked.
  // Doesn't matter how many times we send the signal.
  for(int i = 0; i < 10; ++i) {
    if(kill(pid, SIGUSR1) != 0) {
      abort();
    }
  }

  // Set signal pending in thread. Won't be delivered yet since it's blocked.
  // Doesn't matter how many times we send the signal.
  for(int i = 0; i < 10; ++i) {
    if(syscall(SYS_tgkill, pid, tid, SIGUSR1) != 0) {
      abort();
    }
  }

  // No signals delivered yet, since the signal is blocked.
  if (count != 0) {
    abort();
  }

  // Unblock the signal. The signal will be synchronously delivered *twice*.
  // Since we specified SA_NODEFER, the handler itself gets interrupted by the
  // other instance of the signal, which can be verified in gdb. If we didn't
  // specify SA_NODEFER, the signal would still be delivered twice, but the
  // handler would be allowed to finish processing the first signal before the
  // second is delivered.
  if (sigprocmask(SIG_UNBLOCK, &mask, NULL) != 0) {
    abort();
  }

  // Signal will have been synchronously delivered *twice*, since it was
  // pending at both the process and thread level.
  if (count != 2) {
    abort();
  }

  printf("Final count %d\n", count);
  return 0;
}
