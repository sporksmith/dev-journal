#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

// Not thread safe
void* getHandle() {
    static void* handle = NULL;
    if (handle == NULL) {
        handle = dlopen("./libc.so", RTLD_NOW);
    }
    return handle;
}

ssize_t write(int fd, const void* buf, size_t count) {
    ssize_t (*wrapped_write)(int fd, const void* buf, size_t count) =
        dlsym(getHandle(), "write");
    return wrapped_write(fd, buf, count);
}

size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
    size_t (*wrapped_fwrite)(const void* ptr, size_t size, size_t nmemb,
                             FILE* stream) = dlsym(getHandle(), "fwrite");
    return wrapped_fwrite(ptr, size, nmemb, stream);
}

int printf(const char *format, ...) {
    int (*wrapped_vprintf)(const char* format, va_list ap) =
        dlsym(getHandle(), "vprintf");

    va_list args;
    va_start(args, format);
    int rv = wrapped_vprintf(format, args);
    va_end(args);
    return rv;
}

