#define _GNU_SOURCE

#include <dlfcn.h>
#include <unistd.h>

ssize_t __write(int fd, const void *buf, size_t count) {
    ssize_t (*orig_write)(int fd, const void *buf, size_t count) = 
        dlsym(RTLD_NEXT, "__write");
    orig_write(fd, buf, count);
    return orig_write(fd, buf, count);
}
