# Interposing versioned symbols

Build commands omitted, but can be seen in the accompanying `Makefile`. The
main thing to know is that where a `.map` file is specified, we use it by
passing `-Wl,--version-script=foo.map` in the link step.

## Setup

Suppose we have a program `user_v1`, compiled against v1 of a library
`libtarget_v1.so`:

Executable source:

    $ cat ./user_v1.c
    void target();
    
    int main() {
      target();
    }

Library source:

    $ cat ./target_v1.c
    #include <stdio.h>
    
    void target() {
      printf("Called original v1\n");
    }

Library version map:

    $ cat ./target_v1.map
    TARGET_1_0_0 { target; };

It'll be linked against v1 of `libtarget`'s symbols:

    $ readelf -s libtarget_v1.so | grep target
         7: 0000000000001119    23 FUNC    GLOBAL DEFAULT   15 target@@TARGET_1_0_0
        41: 0000000000000000     0 FILE    LOCAL  DEFAULT  ABS target_v1.c
        55: 0000000000001119    23 FUNC    GLOBAL DEFAULT   15 target

As expected, when we run it, v1 of the `target` function gets called:

    $ LD_LIBRARY_PATH=. ./user_v1
    Called original v1

What happens if we try to interpose `target` via `LD_PRELOAD`? 

## Interposing with an unversioned library

It turns out we can still interpose `target` normally without using symbol
versioning at all.

Interposer source:

    $ cat ./interposer.c
    #include <stdio.h>
    
    void target() {
      printf("Called interposer unversioned\n");
    }

We compile the interposer library without a version map, giving unversioned symbols:

    $ readelf -s libinterposer.so | grep target
         6: 0000000000001119    23 FUNC    GLOBAL DEFAULT   14 target
        53: 0000000000001119    23 FUNC    GLOBAL DEFAULT   14 target

When we use the `libinterposer.so` with `LD_PRELOAD`, our unversioned
symbol still overrides the versioned symbol:

    $ LD_LIBRARY_PATH=. LD_PRELOAD=./libinterposer.so ./user_v1
    Called interposer unversioned

## Interposing specific versions

Suppose the API of `target` in `libtarget` changed in `v2`, and now
returns an integer:

    $ cat ./target_v2.c
    #include <stdio.h>
    
    int target() {
      printf("Called original v2\n");
      return 42;
    }

And now suppose we have an updated version of the `user` program that uses
the newer version:

    $ cat ./user_v2.c
    #include <stdio.h>
    
    int target();
    
    int main() {
      int rv = target();
      printf("User got %d\n", rv);
    }

When run normally, it gets some expected return value:

    $ LD_LIBRARY_PATH=. ./user_v2
    Called original v2
    User got 42

We can still use our old interposer library, but since it implements the old
API with a void return value, the caller will get garbage back:

    $ LD_LIBRARY_PATH=. LD_PRELOAD=./libinterposer.so ./user_v2
    Called interposer unversioned
    User got 30

We could update our library to the new API, but suppose we want it work both
with programs using the older API and the newer API? It turns out we can use
the `symver` instruction to include multiple versions. In the source file we
give each implementation a unique name, and then use `symver` to remap those
names to multiple versions of the original symbol:

    $ cat ./interposer_v1v2.c
    #include <stdio.h>
    
    void target_v1() {
      printf("Called interposer v1 fn\n");
    }
    __asm__(".symver target_v1,target@TARGET_1_0_0");
    
    
    int target_v2() {
      printf("Called interposer v2 fn\n");
      return 42;
    }
    __asm__(".symver target_v2,target@TARGET_2_0_0");

Resulting in a library with multiple versions of the symbol:

    $ readelf -s libinterposer_v1v2.so | grep target
         6: 0000000000001119    23 FUNC    GLOBAL DEFAULT   15 target@TARGET_1_0_0
         7: 0000000000001130    27 FUNC    GLOBAL DEFAULT   15 target@TARGET_2_0_0
         8: 0000000000001119    23 FUNC    GLOBAL DEFAULT   15 target_v1
        11: 0000000000001130    27 FUNC    GLOBAL DEFAULT   15 target_v2
        54: 0000000000001119    23 FUNC    GLOBAL DEFAULT   15 target@TARGET_1_0_0
        57: 0000000000001130    27 FUNC    GLOBAL DEFAULT   15 target_v2
        59: 0000000000001130    27 FUNC    GLOBAL DEFAULT   15 target@TARGET_2_0_0
        61: 0000000000001119    23 FUNC    GLOBAL DEFAULT   15 target_v1

When we run against v1 of the user program, we interpose with our v1 method:

    $ LD_LIBRARY_PATH=. LD_PRELOAD=./libinterposer_v1v2.so ./user_v1
    Called interposer v1 fn

When we run against v2 of the user program, we interpose with our v2 method:

    $ LD_LIBRARY_PATH=. LD_PRELOAD=./libinterposer_v1v2.so ./user_v2
    Called interposer v2 fn
    User got 42

## Unhandled versions

What happens, though, when there's later a v3 of `libtarget`?

    $ cat ./target_v3.c
    #include <stdio.h>
    
    int target() {
      printf("Called original v3\n");
      return 42;
    }

    $ cat ./target_v3.map
    TARGET_3_0_0 { target; };

If we try to use our multi-version interposer library, *neither* of our
implementations match, and the original function gets called.

    $ LD_LIBRARY_PATH=. LD_PRELOAD=./libinterposer_v1v2.so ./user_v3
    Called original v3
    User got 42

We can address this by *also* providing an unversioned symbol. This will get
used if no specific symbol matches. We probably want to enumerate all the old
versions we know about, and then provide a default implementing the new API so
that it'll work with the future versions:

    $ cat ./interposer_v1v2_default.c
    #include <stdio.h>
    
    void target_v1() {
      printf("Called interposer v1 fn\n");
    }
    __asm__(".symver target_v1,target@TARGET_1_0_0");
    
    int target_v2() {
      printf("Called interposer v2 fn\n");
      return 42;
    }
    __asm__(".symver target_v2,target@TARGET_2_0_0");
    
    // We need to write a wrapper for each old version we want to handle:
    void target_v0() {
      target_v1();
    }
    __asm__(".symver target_v0,target@TARGET_0_0_0");
    
    // Provide a default using the new API:
    int target() {
      return target_v2();
    }

Even though we still didn't explicitly add a v3 symbol, it now works with v3:

    $ LD_LIBRARY_PATH=. LD_PRELOAD=./libinterposer_v1v2_default.so ./user_v3
    Called interposer v2 fn
    User got 42

