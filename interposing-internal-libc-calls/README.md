# Interposing internal libc calls

## TLDR

Calls within libc are generally pre-linked. As a result, they can't be interposed using `LD_PRELOAD`. This makes it tricky to use `LD_PRELOAD` to intercept, e.g., all calls to `write`, but there are some workarounds.

## Doubling all writes to standard output

Suppose we want to use `LD_PRELOAD` to force any output to `stdout` to happen twice. We'll start with a simple program that writes to `stdout` once:


```bash
$ cat ./call_write.c
```

    #include <unistd.h>
    #include <string.h>
    
    int main(int argc, char **argv) {
        const char *msg = "Hello write\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        return 0;
    }


When we run `./call_write` we get the expected output:


```bash
$ ./call_write
```

    Hello write


If we examine the compiled binary, we can see that the call is made through the PLT (Procedure Linkage Table):


```bash
$ objdump -D call_write | grep 'call.*write'
```

    call_write:     file format elf64-x86-64
      400577:	e8 a4 fe ff ff       	callq  400420 <write@plt>


This means that at run-time, the dynamic linker is responsible for finding the address of `write` and patching it into the PLT. This `call` instruction will then use the patched table to call the correct function. We can enable debugging-output in the dynamic linker to see that it resolves the `write` symbol to libc, as expected:


```bash
$ LD_DEBUG=bindings ./call_write 2>&1 | grep 'symbol.*write'
```

          7768:	binding file ./call_write [0] to /lib/x86_64-linux-gnu/libc.so.6 [0]: normal symbol `write' [GLIBC_2.2.5]


### Interposing a direct call to write works

We can *interpose* this call to `write` by using the `LD_PRELOAD` environment variable to tell the dynamic linker to try resolving symbols in *our* library that we provide before any other libraries.

Let's create a small library with our own implementation of `write`. We'll simply get the symbol for libc's `write`, and then call it twice.


```bash
$ cat interpose_write.c
```

    #define _GNU_SOURCE
    
    #include <dlfcn.h>
    #include <unistd.h>
    
    ssize_t write(int fd, const void *buf, size_t count) {
        // Look up the next `write` symbol, which will be glibc's
        ssize_t (*orig_write)(int fd, const void *buf, size_t count) = 
            dlsym(RTLD_NEXT, "write");
        if (STDOUT_FILENO) {
            // If we're writing to stdout, call the original function an extra time.
            orig_write(fd, buf, count);
        }
        return orig_write(fd, buf, count);
    }


Using `LD_PRELOAD`, this works as expected. The call to `write` from our `call_fwrite` program is intercepted, and we see the output twice:


```bash
$ LD_PRELOAD=$PWD/interpose_write.so ./call_write
```

    Hello write
    Hello write


We can also examine the dynamic linker's debug output again to see that it resolves `write` twice. First when patching the PLT it finds our `write`. At run-time when we ask it for the "next" `write`, it finds libc's version:


```bash
$ LD_DEBUG=bindings LD_PRELOAD=$PWD/interpose_write.so ./call_write 2>&1 | grep 'symbol.*write'
```

          7783:	binding file ./call_write [0] to /home/jnewsome/projects/dev-journal/interposing-internal-libc-calls/interpose_write.so [0]: normal symbol `write' [GLIBC_2.2.5]
          7783:	binding file /home/jnewsome/projects/dev-journal/interposing-internal-libc-calls/interpose_write.so [0] to /lib/x86_64-linux-gnu/libc.so.6 [0]: normal symbol `write'


## Interposing *all* the writes

So far so good. What happens though if the program we're interposing calls some other libc function to write to standard output? They'll all end up eventually needing to make a `write` system call, so our wrapper should still work... right?

Let's see what happens if the program calls `fwrite` instead of `write`:


```bash
$ cat call_fwrite.c
```

    #include <stdio.h>
    #include <string.h>
    
    int main(int argc, char **argv) {
        const char *msg = "Hello fwrite\n";
        fwrite(msg, 1, strlen(msg), stdout);
        return 0;
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
        // Look up the next `__write` symbol, which will be glibc's
        ssize_t (*orig_write)(int fd, const void *buf, size_t count) = 
            dlsym(RTLD_NEXT, "__write");
        if (STDOUT_FILENO) {
            // If we're writing to stdout, call the original function an extra time.
            orig_write(fd, buf, count);
        }
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

          7796:	binding file ./call_fwrite [0] to /lib/x86_64-linux-gnu/libc.so.6 [0]: normal symbol `fwrite' [GLIBC_2.2.5]


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
     > /home/jnewsome/projects/dev-journal/interposing-internal-libc-calls/call_fwrite(main+0x5b) [0x5cb]
     > /lib/x86_64-linux-gnu/libc-2.27.so(__libc_start_main+0xe7) [0x21b97]
     > /home/jnewsome/projects/dev-journal/interposing-internal-libc-calls/call_fwrite(_start+0x2a) [0x4aa]
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

* Interpose every entry point into glibc that could end up calling the functionality we want to modify. This could work, but developing and maintaining 100% coverage is difficult and fragile. We'd also end up having to reimplement parts of libc. e.g. if we write a wrapper for `fwrite`, and want to interpose on the individual `write` system calls, we'd have to reimplement the in-memory buffering that `fwrite` puts in between the two ourselves (or forgo it).
* Patch glibc to make calls via the PLT, so that we can interpose them via `LD_PRELOAD`. We could then inject our custom glibc as another `LD_PRELOAD` library as done [here](../patching-libc-to-interpose-syscalls). This seems like a promising shortcut, but could result in subtle issues if our patched glibc isn't binary-compatible with the headers that the target code was compiled against; e.g. uses different data type or constant definitions. We should be able to prevent those difficulties by being very careful to build our libc consistently with our distribution's libc, but we'd need to do so for every platform we intend to run on.
* Use a different interposition mechanism than `LD_PRELOAD`. In particular, `ptrace` tells the OS kernel to intercept the system calls themselves, ensuring that we see every system call regardless of what function it's coming from, whether the program is statically linked, or even if the `syscall` instructions themselves are inlined. This works best when the behavior we're trying to modify is at the syscall level. It's a bit more work to implement, though. It may also have different performance characteristics, especially if there are other syscalls we're *not* interested in intercepting. We also can't attach a debugger to a program that is already being `ptrace`'d, making debugging more difficult.
