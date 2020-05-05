# Interposing internal libc calls

## TLDR

Calls within libc are generally pre-linked. As a result, they can't be interposed using `LD_PRELOAD`. This makes it tricky to use `LD_PRELOAD` to intercept, e.g., all calls to `write`, but there are some workarounds.

## An example program

For demo purposes we'll use a program that makes a single call to `fwrite`:


```bash
$ cat ./call_fwrite.c
```

    #include <stdio.h>
    #include <string.h>
    
    int main(int argc, char **argv) {
        const char *msg = "Hello fwrite\n";
        fwrite(msg, 1, strlen(msg), stdout);
        return 0;
    }


When we run `./call_fwrite` we get the expected output:


```bash
$ ./call_fwrite
```

    Hello fwrite


If we examine the compiled binary, we can see that the call is made through the PLT (Procedure Linkage Table):


```bash
$ objdump -D call_fwrite | grep 'call.*fwrite'
```

    call_fwrite:     file format elf64-x86-64
      4005b6:	e8 a5 fe ff ff       	callq  400460 <fwrite@plt>


This means that at run-time, the dynamic linker is responsible for finding the address of `fwrite` and patching it into the PLT. This `call` instruction will then use the patched table to call the correct function. We can enable debugging-output in the dynamic linker to see that it resolves the `fwrite` symbol to libc, as expected:


```bash
$ LD_DEBUG=bindings ./call_fwrite 2>&1 | grep 'symbol.*fwrite'
```

         11204:	binding file ./call_fwrite [0] to /lib/x86_64-linux-gnu/libc.so.6 [0]: normal symbol `fwrite' [GLIBC_2.2.5]


### Interposing fwrite itself works

We can *interpose* this call to `fwrite` by using the `LD_PRELOAD` environment variable to tell the dynamic linker to try resolving symbols in *our* library that we provide before any other libraries.

Let's create a small library with our own implementation of `fwrite`. We'll simply get the symbol for libc's `fwrite`, and then call it twice.


```bash
$ cat interpose_fwrite.c
```

    #define _GNU_SOURCE
    
    #include <stdio.h>
    #include <dlfcn.h>
    
    size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
        size_t (*orig_fwrite)(const void* ptr, size_t size, size_t nmemb,
                              FILE* stream) = dlsym(RTLD_NEXT, "fwrite");
        orig_fwrite(ptr, size, nmemb, stream);
        return orig_fwrite(ptr, size, nmemb, stream);
    }


Using `LD_PRELOAD`, this works as expected. The call to `fwrite` from our `call_fwrite` program is intercepted, and we see the output twice:


```bash
$ LD_PRELOAD=$PWD/interpose_fwrite.so ./call_fwrite
```

    Hello fwrite
    Hello fwrite


We can also examine the dynamic linker's debug output again to see that it resolves `fwrite` twice. First when patching the PLT it finds our `fwrite`. At run-time when we ask it for the "next" `fwrite`, it finds libc's version:


```bash
$ LD_DEBUG=bindings LD_PRELOAD=$PWD/interpose_fwrite.so ./call_fwrite 2>&1 | grep 'symbol.*fwrite'
```

         11208:	binding file ./call_fwrite [0] to /home/jnewsome/projects/dev-journal/interposing-fwrite/interposing-fwrite/interpose_fwrite.so [0]: normal symbol `fwrite' [GLIBC_2.2.5]
         11208:	binding file /home/jnewsome/projects/dev-journal/interposing-fwrite/interposing-fwrite/interpose_fwrite.so [0] to /lib/x86_64-linux-gnu/libc.so.6 [0]: normal symbol `fwrite'


## Interposing *all* the writes

So far so good. But suppose we want to apply our doubling to *all* writes. There are a lot of different libc functions that end up writing data; e.g. `printf`, `fprintf`, `puts`, ... We could identify the full list and override all of them in our library, but it could be tricky to identify the full set of such functions, especially since in practice some of them are actually macros that call internal libc symbols rather than the public libc api. Depending what exactly we're trying to do, it'd also potentially result in some duplication of our custom logic in each of these, and potentially force us to reimplement some of the functionality in the wrapped functions.

Ultimately these all end up making a `write` system call. Maybe we can just interpose the `write` function and put our logic there?


```bash
$ cat interpose_write.c
```

    #define _GNU_SOURCE
    
    #include <dlfcn.h>
    #include <unistd.h>
    
    ssize_t write(int fd, const void *buf, size_t count) {
        ssize_t (*orig_write)(int fd, const void *buf, size_t count) = 
            dlsym(RTLD_NEXT, "write");
        orig_write(fd, buf, count);
        return orig_write(fd, buf, count);
    }



```bash
$ LD_PRELOAD=$PWD/interpose_write.so ./call_fwrite
```

    Hello fwrite


It didn't work. To find out why, let's look at calls to `write` from within libc:


```bash
$ objdump -D /lib/x86_64-linux-gnu/libc.so.6 | grep 'call.*__write@@'
```

       21cc5:	e8 76 e4 0e 00       	callq  110140 <__write@@GLIBC_2.2.5>
       303ab:	e8 90 fd 0d 00       	callq  110140 <__write@@GLIBC_2.2.5>
       7c79a:	e8 41 3a 09 00       	callq  1101e0 <__write@@GLIBC_2.2.5+0xa0>
       8b1b8:	e8 83 4f 08 00       	callq  110140 <__write@@GLIBC_2.2.5>
       8b1e0:	e8 fb 4f 08 00       	callq  1101e0 <__write@@GLIBC_2.2.5+0xa0>
      1154af:	e8 8c ac ff ff       	callq  110140 <__write@@GLIBC_2.2.5>
      1175bd:	e8 1e 8c ff ff       	callq  1101e0 <__write@@GLIBC_2.2.5+0xa0>
      121ae3:	e8 58 e6 fe ff       	callq  110140 <__write@@GLIBC_2.2.5>
      123965:	e8 76 c8 fe ff       	callq  1101e0 <__write@@GLIBC_2.2.5+0xa0>
      124059:	e8 82 c1 fe ff       	callq  1101e0 <__write@@GLIBC_2.2.5+0xa0>
      13aae4:	e8 57 56 fd ff       	callq  110140 <__write@@GLIBC_2.2.5>
      13aca3:	e8 98 54 fd ff       	callq  110140 <__write@@GLIBC_2.2.5>
      13adc2:	e8 79 53 fd ff       	callq  110140 <__write@@GLIBC_2.2.5>
      13b840:	e8 fb 48 fd ff       	callq  110140 <__write@@GLIBC_2.2.5>
      13b982:	e8 b9 47 fd ff       	callq  110140 <__write@@GLIBC_2.2.5>
      13ba04:	e8 37 47 fd ff       	callq  110140 <__write@@GLIBC_2.2.5>
      14e7ec:	e8 4f 19 fc ff       	callq  110140 <__write@@GLIBC_2.2.5>
      156365:	e8 d6 9d fb ff       	callq  110140 <__write@@GLIBC_2.2.5>
      15aa25:	e8 16 57 fb ff       	callq  110140 <__write@@GLIBC_2.2.5>
      163c6e:	e8 6d c5 fa ff       	callq  1101e0 <__write@@GLIBC_2.2.5+0xa0>
      164884:	e8 57 b9 fa ff       	callq  1101e0 <__write@@GLIBC_2.2.5+0xa0>
      164a51:	e8 8a b7 fa ff       	callq  1101e0 <__write@@GLIBC_2.2.5+0xa0>


It turns out that none of the internal calls to `write` actually use the `write` symbol. They use `__write`, which is an alias:



```bash
$ __write_address=`nm -D /lib/x86_64-linux-gnu/libc.so.6 | awk '/__write/ {print $1}'`
$ nm -D /lib/x86_64-linux-gnu/libc.so.6 | grep $__write_address
```

    0000000000110140 W write
    0000000000110140 W __write


Unfortunately, simply overriding `__write` doesn't work either:


```bash
$ cat interpose_underbar_write.c
```

    #define _GNU_SOURCE
    
    #include <dlfcn.h>
    #include <unistd.h>
    
    ssize_t __write(int fd, const void *buf, size_t count) {
        ssize_t (*orig_write)(int fd, const void *buf, size_t count) = 
            dlsym(RTLD_NEXT, "__write");
        orig_write(fd, buf, count);
        return orig_write(fd, buf, count);
    }



```bash
$ LD_PRELOAD=$PWD/interpose_underbar_write.so ./call_fwrite
```

    Hello fwrite


Note that in the `call` instructions above, the symbols don't have the `@plt` suffix. The real issue is that none of these calls are via the PLT. When glibc itself is built and linked, the linker can see the call sites and intended destinations at the same time, so it can and does link them at that time. The *dynamic* linker isn't involved, so `LD_PRELOAD` has no effect on these call sites.

Looking at the dynamic linker's debug output again, we can see it never looks up `write` or `__write` at all; only `fwrite`:


```bash
$ LD_DEBUG=bindings LD_PRELOAD=$PWD/interpose_underbar_write.so ./call_fwrite 2>&1 | grep 'symbol.*write'
```

         11221:	binding file ./call_fwrite [0] to /lib/x86_64-linux-gnu/libc.so.6 [0]: normal symbol `fwrite' [GLIBC_2.2.5]


As it turns out, that may not be the *only* issue. It turns out that the `write` symbol itself involved at all in this case. We can use `strace` to get the call stack where the `write` system call gets made in our program:

## Can we interpose some other function?

Using `strace`, we can get the call stack when the `write` system call is made. Maybe we can interpose one of these other functions?


```bash
$ strace -k -e write ./call_fwrite 2>&1
```

    write(1, "Hello fwrite\n", 13Hello fwrite
    )          = 13
     > /lib/x86_64-linux-gnu/libc-2.27.so(__write+0x14) [0x110154]
     > /lib/x86_64-linux-gnu/libc-2.27.so(_IO_file_write+0x2d) [0x8b1bd]
     > /lib/x86_64-linux-gnu/libc-2.27.so(_IO_do_write+0xb1) [0x8cf51]
     > /lib/x86_64-linux-gnu/libc-2.27.so(_IO_file_overflow+0x103) [0x8d403]
     > /lib/x86_64-linux-gnu/libc-2.27.so(_IO_default_xsputn+0x74) [0x8e494]
     > /lib/x86_64-linux-gnu/libc-2.27.so(_IO_file_xsputn+0x103) [0x8ba33]
     > /lib/x86_64-linux-gnu/libc-2.27.so(fwrite+0xd7) [0x7f977]
     > /home/jnewsome/projects/dev-journal/interposing-fwrite/interposing-fwrite/call_fwrite(main+0x5b) [0x5bb]
     > /lib/x86_64-linux-gnu/libc-2.27.so(__libc_start_main+0xe7) [0x21b97]
     > /home/jnewsome/projects/dev-journal/interposing-fwrite/interposing-fwrite/call_fwrite(_start+0x2a) [0x49a]
    +++ exited with 0 +++


Maybe one of these ends up getting called via the PLT. Let's see what function *are* called via the PLT inside the glibc library:


```bash
$ objdump -D /lib/x86_64-linux-gnu/libc.so.6 | grep -o '<.*@plt>' | sort | uniq
```

    <*ABS*+0x9d790@plt>
    <*ABS*+0x9d7c0@plt>
    <*ABS*+0x9d800@plt>
    <*ABS*+0x9d840@plt>
    <*ABS*+0x9d870@plt>
    <*ABS*+0x9dc70@plt>
    <*ABS*+0x9dca0@plt>
    <*ABS*+0x9dd00@plt>
    <*ABS*+0x9dd40@plt>
    <*ABS*+0x9dd70@plt>
    <*ABS*+0x9dda0@plt>
    <*ABS*+0x9e050@plt>
    <*ABS*+0x9ebe0@plt>
    <*ABS*+0x9ec10@plt>
    <*ABS*+0x9ec70@plt>
    <*ABS*+0x9ed40@plt>
    <*ABS*+0x9ede0@plt>
    <*ABS*+0x9eef0@plt>
    <*ABS*+0x9ef20@plt>
    <*ABS*+0x9ef50@plt>
    <*ABS*+0x9efa0@plt>
    <*ABS*+0x9eff0@plt>
    <*ABS*+0x9f040@plt>
    <*ABS*+0x9f0e0@plt>
    <*ABS*+0xa07c0@plt>
    <*ABS*+0xa07f0@plt>
    <*ABS*+0xa86c0@plt>
    <*ABS*+0xbc0f0@plt>
    <*ABS*+0xbceb0@plt>
    <*ABS*+0xbd410@plt>
    <*ABS*+0xbd4c0@plt>
    <*ABS*+0xbe6c0@plt>
    <*ABS*+0xd2980@plt>
    <*ABS*+0xd2a60@plt>
    <calloc@plt>
    <_dl_exception_create@plt>
    <_dl_find_dso_for_object@plt>
    <free@plt>
    <malloc@plt>
    <memalign@plt>
    <realloc@plt>
    <__tls_get_addr@plt>
    <__tunable_get_val@plt>


As it turns out, in a normal glibc build, only the memory allocation functions and some opaque implementation details are called via the PLT. Presumably the memory allocation functions are called via the PLT because they're explicitly meant to be overridable. I'm not sure what the other things are. Notably absent are any of the symbols in the `__write` call stack, above.

## So what *can* we do?

There are a few way around this.

* Interpose every entry point into glibc that could end up calling the functionality we want to modify. This could work, but developing and maintaining 100% coverage is difficult. We'd also end up having to reimplement parts of libc. e.g. if we write a wrapper for `fwrite`, and want to interpose on the individual `write` system calls, we'd have to reimplement the in-memory buffering that `fwrite` puts in between the two ourselves (or forgo it).
* Patch glibc to make calls via the PLT, so that we can interpose them via `LD_PRELOAD`. We could then inject our custom glibc as another `LD_PRELOAD` library as done [here](https://github.com/sporksmith/interpose-demo). This seems like a promising shortcut, but could result in subtle issues if our patched glibc isn't binary-compatible with the headers that the target code was compiled against; e.g. uses different `struct` or constant definitions. We could prevent those difficulties by recompiling the target software (and its dependencies, recursively) against our patched glibc and its headers, but this negates the usual advantage of `LD_PRELOAD` techniques of not having to recompile.
* Use a different interposition mechanism than `LD_PRELOAD`. In particular, `ptrace` tells the OS kernel to intercept the system calls themselves, ensuring that we see every system call regardless of what function it's coming from, whether the program is statically linked, or even if the `syscall` instructions themselves are inlined. This works best when the behavior we're trying to modify is at the syscall level. It may also have different performance characteristics though, especially if there are other syscalls we're *not* interested in intercepting. We also can't attach a debugger to a program that is already being `ptrace`'d, making debugging more difficult.
