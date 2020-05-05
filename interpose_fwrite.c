#define _GNU_SOURCE

#include <stdio.h>
#include <dlfcn.h>

size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
    size_t (*orig_fwrite)(const void* ptr, size_t size, size_t nmemb,
                          FILE* stream) = dlsym(RTLD_NEXT, "fwrite");
    orig_fwrite(ptr, size, nmemb, stream);
    return orig_fwrite(ptr, size, nmemb, stream);
}
