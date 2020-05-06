#define _GNU_SOURCE

#include <stdarg.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>

static long real_syscall(long n, long arg1, long arg2, long arg3, long arg4,
                         long arg5, long arg6) {
    long rv;
    asm volatile(
        "movq %[n], %%rax\n"
        "movq %[arg1], %%rdi\n"
        "movq %[arg2], %%rsi\n"
        "movq %[arg3], %%rdx\n"
        "movq %[arg4], %%r10\n"
        "movq %[arg5], %%r8\n"
        "movq %[arg6], %%r9\n"
        "syscall\n"
        "movq %%rax, %[rv]\n"
        : /* output parameters*/
        [rv] "=rm"(rv)
        : /* input parameters */
        [n] "rm"(n), [arg1] "rm"(arg1), [arg2] "rm"(arg2), [arg3] "rm"(arg3),
        [arg4] "rm"(arg4), [arg5] "rm"(arg5), [arg6] "rm"(arg6)
        : /* other clobbered regs */
        "rdi",
        // "SYSCALL also saves RFLAGS into R11"
        "r11",
        // "The SYSCALL instruction does not save the stack pointer (RSP)"
        "rsp",
        // Used to save rip; unclear whether it gets restored"
        "rcx",
        // Flags
        "cc",
        // Memory
        "memory");
    return rv;
}

long syscall(long n, ...) {
    va_list args;
    va_start(args, n);
    long arg1 = va_arg(args, long);
    long arg2 = va_arg(args, long);
    long arg3 = va_arg(args, long);
    long arg4 = va_arg(args, long);
    long arg5 = va_arg(args, long);
    long arg6 = va_arg(args, long);
    va_end(args);

    if (n == SYS_write && arg1 == STDOUT_FILENO) {
        real_syscall(n, arg1, arg2, arg3, arg4, arg5, arg6);
    }
    return real_syscall(n, arg1, arg2, arg3, arg4, arg5, arg6);
}

