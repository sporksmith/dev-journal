#define _GNU_SOURCE

#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    const char* msg = "syscall\n";
    syscall(SYS_write, STDOUT_FILENO, msg, strlen(msg));

    msg = "write\n";
    write(STDOUT_FILENO, msg, strlen(msg));

    msg = "fwrite\n";
    fwrite(msg, 1, strlen(msg), stdout);

    printf("printf\n");
    fprintf(stdout, "fprintf\n");
    puts("puts");
    fputs("fputs\n", stdout);
    putc('!', stdout);
    putc('\n', stdout);
}
