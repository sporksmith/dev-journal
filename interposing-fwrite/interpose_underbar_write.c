#define _GNU_SOURCE

#include <dlfcn.h>
#include <unistd.h>

ssize_t __write(int fd, const void *buf, size_t count) {
    // Look up the next `__write` symbol, which will be glibc's
    ssize_t (*orig_write)(int fd, const void *buf, size_t count) = 
        dlsym(RTLD_NEXT, "__write");
    if (STDOUT_FILENO) {
        // If we're writing to stdout, call the original function an extra time.
        orig_write(fd, buf, count);
    }
    return orig_write(fd, buf, count);
}
