#define _GNU_SOURCE

#include <linux/filter.h>
#include <linux/seccomp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/ucontext.h>
#include <unistd.h>

#ifndef SS_AUTODISARM
#define SS_AUTODISARM (1U << 31)
#endif

static long _syscall(long n, ...) {
    va_list args;
    va_start(args, n);
    long arg1 = va_arg(args, long);
    long arg2 = va_arg(args, long);
    long arg3 = va_arg(args, long);
    long arg4 = va_arg(args, long);
    long arg5 = va_arg(args, long);
    long arg6 = va_arg(args, long);
    va_end(args);

    long rv;

    register long r10 __asm__("r10") = arg4;
    register long r8 __asm__("r8") = arg5;
    register long r9 __asm__("r9") = arg6;
    __asm__ __volatile__("syscall"
                         : "=a"(rv)
                         : "a"(n), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10), "r"(r8), "r"(r9)
                         : "rcx", "r11", "memory");
    return rv;
}

static void _handle_sigsys(int signo, siginfo_t* info, void* voidUcontext) {
  ucontext_t* ctx = (ucontext_t*)(voidUcontext);
  greg_t* regs = ctx->uc_mcontext.gregs;

  char buf[100];
  // XXX not guaranteed as signal-safe.
  sprintf(buf, "Trapped syscall %lld\n", regs[REG_RAX]);
  _syscall(SYS_write, 2, buf, strlen(buf));

  long n = regs[REG_RAX];
  long args[6] = {regs[REG_RDI], regs[REG_RSI], regs[REG_RDX], regs[REG_R10], regs[REG_R8], regs[REG_R9]};

  // Don't allow overwriting the SIGSYS handler.
  if (n == SYS_rt_sigaction && args[0] == SIGSYS) {
    args[1] = 0;
  }

  // Don't allow masking SIGSYS.
  // Using implementation details of the kernel's definition of sigset (as a 64 bit bitfield) here.
  // Careful not to mutate the passed in pointer, which would break const correctness.
  uint64_t alt_sigset;
  if (n == SYS_rt_sigprocmask) {
    int how = args[0];
    const uint64_t *set = (const uint64_t*)args[1];
    if (set != NULL && (how == SIG_BLOCK || how == SIG_SETMASK)) {
      alt_sigset = *set & ~(UINT64_C(1)<<(SIGSYS-1));
      args[1] = (long)&alt_sigset;
    }
  }

  regs[REG_RAX] = _syscall(n, args[0], args[1], args[2], args[3], args[4], args[5]);
}

static void _setup_sigaltstack() {
  static __thread char stack_buf[8 * 1<<20];
  stack_t stack = {
    .ss_sp = stack_buf,
    .ss_size = sizeof(stack_buf),
    .ss_flags = SS_AUTODISARM,
  };
  if (sigaltstack(&stack, NULL) != 0) {
    abort();
  }
}

__attribute__((constructor)) static void load() {
  _setup_sigaltstack();
  
  if (sigaction(SIGSYS,
        &(struct sigaction){
          .sa_sigaction = _handle_sigsys,
          .sa_flags = SA_NODEFER | SA_SIGINFO | SA_ONSTACK,
          }, NULL) != 0) {
    abort();
  }

  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
      abort();
  }

  struct sock_filter filter[] = {
        /* accumulator := syscall number */
        BPF_STMT(BPF_LD + BPF_W + BPF_ABS, offsetof(struct seccomp_data, nr)),

        /* Always allow sigreturn; otherwise we'd crash returning from our signal handler. */
        BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, SYS_rt_sigreturn, /*true-skip=*/0, /*false-skip=*/1),
        BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_ALLOW),

        /* Always allow sigaltstack; we can't call it from the signal handler */
        BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, SYS_sigaltstack, /*true-skip=*/0, /*false-skip=*/1),
        BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_ALLOW),

        /* XXX allow clone. we'll need to handle it specially, but skip for now. */
        BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, SYS_clone, /*true-skip=*/0, /*false-skip=*/1),
        BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_ALLOW),

        /* See if instruction pointer is within the _syscall fn. */
        /* accumulator := instruction_pointer */
        BPF_STMT(BPF_LD + BPF_W + BPF_ABS, offsetof(struct seccomp_data, instruction_pointer)),
        /* If it's in `_syscall`, allow. We don't know the end address, but it
         * should be safe-ish to check if it's within a kilobyte or so. We know there are no
         * other syscall instructions within this library, so the only problem would be if
         * shim_native_syscallv ended up at the very end of the library object, and a syscall
         * ended up being made from the very beginning of another library object, loaded just
         * after ours.
         *
         * TODO: Consider using the actual bounds of this object file, from /proc/self/maps. */
        BPF_JUMP(BPF_JMP + BPF_JGT + BPF_K, ((long)_syscall) + 2000,
                 /*true-skip=*/2, /*false-skip=*/0),
        BPF_JUMP(BPF_JMP + BPF_JGE + BPF_K, (long)_syscall, /*true-skip=*/0,
                 /*false-skip=*/1),
        BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_ALLOW),

        /* Trap to our syscall handler */
        BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_TRAP),
      };
        struct sock_fprog prog = {
        .len = (unsigned short)(sizeof(filter) / sizeof(filter[0])),
        .filter = filter,
    };
    if (syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_SPEC_ALLOW, &prog)) {
        abort();
    }
}