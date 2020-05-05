# Interposing internal libc calls

## TLDR

Calls within libc are generally pre-linked. As a result, they can't be interposed using `LD_PRELOAD`. This makes it tricky to use `LD_PRELOAD` to intercept, e.g., all calls to `write`. One workaround is to [patch libc](https://github.com/sporksmith/interpose-demo).

## An example program

For demo purposes we'll use a program that makes a single call to `fwrite`:


```bash
cat ./fwrite.c
```

    #include <stdio.h>
    #include <string.h>
    
    int main(int argc, char **argv) {
        const char *msg = "Hello fwrite\n";
        fwrite(msg, 1, strlen(msg), stdout);
        return 0;
    }


### Interposing fwrite itself works

Let's look at what happens when interposition works as expected. We'll interpose on `fwrite` itself, to make each such call twice.


```bash
cat interpose_fwrite.c
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



```bash
LD_PRELOAD=$PWD/interpose_fwrite.so ./fwrite
```

    Hello fwrite
    Hello fwrite


This works because the call to fwrite happens via the PLT:


```bash
objdump -D fwrite | grep 'call.*fwrite'
```

      4005b6:	e8 a5 fe ff ff       	[01;31m[Kcallq  400460 <fwrite[m[K@plt>


We can also turn on the dynamic linker's debug output to see this binding happen. Without `LD_PRELOAD`, `fwrite` gets bound to libc's symbol:


```bash
LD_DEBUG=bindings ./fwrite 2>&1 | grep 'symbol.*fwrite'
```

          6988:	binding file ./fwrite [0] to /lib/x86_64-linux-gnu/libc.so.6 [0]: normal [01;31m[Ksymbol `fwrite[m[K' [GLIBC_2.2.5]


With `LD_PRELOAD`, it gets bound to our preloaded library instead:


```bash
LD_DEBUG=bindings LD_PRELOAD=$PWD/interpose_fwrite.so ./fwrite 2>&1 | grep 'symbol.*fwrite'
```

          6990:	binding file ./fwrite [0] to /home/jnewsome/projects/dev-journal/interposing-fwrite/interposing-fwrite/interpose_fwrite.so [0]: normal [01;31m[Ksymbol `fwrite[m[K' [GLIBC_2.2.5]
          6990:	binding file /home/jnewsome/projects/dev-journal/interposing-fwrite/interposing-fwrite/interpose_fwrite.so [0] to /lib/x86_64-linux-gnu/libc.so.6 [0]: normal [01;31m[Ksymbol `fwrite[m[K'


## Interposing write alone doesn't work

So far so good. But if we want to apply our doubling to *all* writes, there are a bunch of other output functions we'll need to intercept. e.g. `printf`, `fprintf`, `puts`, ... Can we do so without writing wrappers for all of these and duplicating our custom logic in all of them?

Ultimately these all end up making a `write` system call. Maybe we can just interpose the `write` function and put our logic there?


```bash
cat interpose_write.c
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
LD_PRELOAD=$PWD/interpose_write.so ./fwrite
```

    Hello fwrite


It didn't work. To find out why, let's look at calls to write from within libc:


```bash
objdump -D /lib/x86_64-linux-gnu/libc.so.6 | grep 'call.*fwrite'
```

       7bcda:	e8 c1 3b 00 00       	[01;31m[Kcallq  7f8a0 <_IO_fwrite[m[K@@GLIBC_2.2.5>
       7cbc4:	e8 d7 2c 00 00       	[01;31m[Kcallq  7f8a0 <_IO_fwrite[m[K@@GLIBC_2.2.5>
       7cbe4:	e8 b7 2c 00 00       	[01;31m[Kcallq  7f8a0 <_IO_fwrite[m[K@@GLIBC_2.2.5>
       9aae6:	e8 b5 4d fe ff       	[01;31m[Kcallq  7f8a0 <_IO_fwrite[m[K@@GLIBC_2.2.5>
       9c9bd:	e8 de 2e fe ff       	[01;31m[Kcallq  7f8a0 <_IO_fwrite[m[K@@GLIBC_2.2.5>
       9cb20:	e8 7b 2d fe ff       	[01;31m[Kcallq  7f8a0 <_IO_fwrite[m[K@@GLIBC_2.2.5>
      11e33d:	e8 7e c1 f6 ff       	[01;31m[Kcallq  8a4c0 <fwrite[m[K_unlocked@@GLIBC_2.2.5>
      11e4bd:	e8 fe bf f6 ff       	[01;31m[Kcallq  8a4c0 <fwrite[m[K_unlocked@@GLIBC_2.2.5>
      15e1af:	e8 ec 16 f2 ff       	[01;31m[Kcallq  7f8a0 <_IO_fwrite[m[K@@GLIBC_2.2.5>
      15e210:	e8 8b 16 f2 ff       	[01;31m[Kcallq  7f8a0 <_IO_fwrite[m[K@@GLIBC_2.2.5>
      15e396:	e8 05 15 f2 ff       	[01;31m[Kcallq  7f8a0 <_IO_fwrite[m[K@@GLIBC_2.2.5>


Note that unlike our program's call to `fwrite`, these don't have the `@plt` suffix. This is because glibc's build process effectively pre-links these calls.

The dynamic linker isn't involved in resolving these calls at all:


```bash
LD_DEBUG=bindings LD_PRELOAD=$PWD/interpose_write.so ./fwrite 2>&1 | grep 'symbol.*[^f]write' || true
```

## Could we be interposing the wrong function?

As it turns out, that may not be the *only* issue. It turns out that the `write` symbol itself involved at all in this case. We can use `strace` to get the call stack where the `write` system call gets made in our program:


```bash
strace -k -e write ./fwrite 2>&1
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
     > /home/jnewsome/projects/dev-journal/interposing-fwrite/interposing-fwrite/fwrite(main+0x5b) [0x5bb]
     > /lib/x86_64-linux-gnu/libc-2.27.so(__libc_start_main+0xe7) [0x21b97]
     > /home/jnewsome/projects/dev-journal/interposing-fwrite/interposing-fwrite/fwrite(_start+0x2a) [0x49a]
    +++ exited with 0 +++


### We actually want `__write`, but it doesn't matter

`strace` resolves the symbol to `__write`. As it turns out this is an alias to the `write` symbol. (And the only such alias):


```bash
__write_address=`nm -D /lib/x86_64-linux-gnu/libc.so.6 | awk '/__write/ {print $1}'`
nm -D /lib/x86_64-linux-gnu/libc.so.6 | grep $__write_address
```

    [01;31m[K0000000000110140[m[K W write
    [01;31m[K0000000000110140[m[K W __write


It's not clear `strace` actually knows what symbol is being used at the call site; it might just be doing a reverse lookup of the address on the stack and happening to pick `__write` between the two aliases.

Looking at the objdump of `_IO_file_write`, it turns out that it does indeed use the `__write` symbol, but again it doesn't happen via the PLT:


```bash
objdump -D /lib/x86_64-linux-gnu/libc.so.6 | awk '/^\S+ <_IO_file_write/ { found=1 }; /call.*write/ { if (found==1) { print $0; exit 0; } }'
```

       8b1b8:	e8 83 4f 08 00       	callq  110140 <__write@@GLIBC_2.2.5>


Just to be sure, let's try interposing `__write`:


```bash
cat interpose_underbar_write.c
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
LD_PRELOAD=$PWD/interpose_underbar_write.so ./fwrite
```

    Hello fwrite



```bash
LD_DEBUG=bindings LD_PRELOAD=$PWD/interpose_underbar_write.so ./fwrite 2>&1 | grep 'symbol.*write'
```

          7011:	binding file ./fwrite [0] to /lib/x86_64-linux-gnu/libc.so.6 [0]: normal [01;31m[Ksymbol `fwrite[m[K' [GLIBC_2.2.5]


As expected: there's no observable effect, and the dynamic loader never touches the `__write` symbol at all.

### glibc only calls a few functions via the PLT

Maybe we could interpose one of those other helpers in the stack instead? Are any of those called via the PLT?

As it turns out, in a normal glibc build, only the memory allocation functions and some opaque implementation details are called via the PLT. Presumably the memory allocation functions are called via the PLT because they're explicitly meant to be overridable. I'm not sure what the other things are.


```bash
objdump -D /lib/x86_64-linux-gnu/libc.so.6 | grep -o '<.*@plt>' | sort | uniq
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


## So what *can* we do?

There are a few way around this.

* Interpose every entry point into glibc that could end up calling the functionality we want to modify. This could work, but developing and maintaining 100% coverage is difficult. We'd also end up having to reimplement parts of libc. e.g. if we write a wrapper for `fwrite`, and want to interpose on the individual `write` system calls, we'd have to reimplement the in-memory buffering that `fwrite` puts in between the two ourselves (or forgo it).
* Patch glibc to make calls via the PLT. We could then inject our custom glibc as another `LD_PRELOAD` library as done [here](https://github.com/sporksmith/interpose-demo). This seems like a promising shortcut, but could result in subtle issues if our patched glibc isn't binary-compatible with the headers that the target code was compiled against; e.g. uses different `struct` or constant definitions. We could prevent those difficulties by recompiling the target software (and its dependencies, recursively) against our patched glibc and its headers, but this negates the usual advantage of `LD_PRELOAD` techniques of not having to recompile.
* Use a different interposition mechanism than `LD_PRELOAD`. In particular, `ptrace` tells the OS kernel to intercept the system calls themselves, ensuring that we see every system call regardless of what function it's coming from, whether the program is statically linked, or even if the `syscall` instructions themselves are inlined. This works best when the behavior we're trying to modify is at the syscall level. It may also have different performance characteristics though, especially if there are other syscalls we're *not* interested in intercepting. We also can't attach a debugger to a program that is already being `ptrace`'d, making debugging more difficult.
