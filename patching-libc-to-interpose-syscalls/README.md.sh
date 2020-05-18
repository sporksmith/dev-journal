#!/bin/bash

show_cmd () {
  echo "    \$ $1"
  bash -c "$1" | sed 's/^/    /'
}

cat <<EOF
title: Patching glibc to make its syscalls interposable
date: 2020-05-04
slug: patching-glibc-to-make-syscalls-interposable

# Patching glibc to make its syscalls interposable

Suppose we have a program, call_write.c, that writes some strings to stdout:

`show_cmd "cat ./call_write.c"`

Output of call_write:

`show_cmd "./call_write"`

Suppose we want to double all output to stdout.  All of the functions in
call_write ultimately make a write syscall, so we interpose the syscall
function directly.

`show_cmd "cat ./interpose.c"`

Unfortunately, it turns out that the libc implementations of these functions
make *inlined* syscalls, so this only successfully interposes on and doubles
the actual call to syscall:

`show_cmd 'LD_PRELOAD=$PWD/libinterpose.so ./call_write'`

We can fix this by using a patched libc that replaces inlined syscalls with
calls to the syscall function, and also LD_PRELOAD'ing that. It turns out we
primarily just need to [redefine some syscall
macros](https://github.com/sporksmith/glibc/commit/6d667159940450ba1ce40b5ea00e8a88a4f7fe21).
When using the library as an LD_PRELOAD I initially got some crashes in code
that tries to do a dynamic symbol lookup to determine whether it's not the
primary libc in use; I worked around by effectively
[hard-coding the answer to "yes"](https://github.com/sporksmith/glibc/commit/575ea9f2412905a323cd0c3c380f003bb9e61e67)

`show_cmd 'LD_PRELOAD=$PWD/libinterpose.so:$PWD/glibc-build/libc.so ./call_write'`

You may wonder why we don''t see the two, e.g. ``fwrite`` outputs directly next
to each-other.  This is because the functions that operate on the stdout
``FILE*`` stream, rather than directly on its file descriptor, write to an
in-memory buffer. i.e. the corresponding writes get batched into a single
``write`` syscall.

# Caveats

We'll run into subtle errors if our preloaded libc uses different data type or
constant definitions than the libc against which the target program was
compiled. Hopefully using the same implementation of libc (glibc, in this
case), with a "close enough" version is sufficient. To be really sure though
we'd need to patch the source of our distribution's libc, which may itself be
patched, and be sure to use the same configuration and toolchain that our
distribution used when building the libc it uses and distributes.

The workaround for avoiding the extra dynamic symbol lookups essentially
tells glibc to always use ``mmap`` instead of ``brk`` for allocating memory.
This will have some performance impact. Since the system's glibc shouldn't end
up getting used at all, we *might* be able to hard-code the opposite default
itself and tell it to ahead and use ``brk``. For this proof-of-concept
I just went with the more conservative approach.

I wouldn't be terribly surprised if we run into other issues at runtime similar
to those dynamic symbol lookup crashes. glibc wasn't designed to be used in
quite this way.
EOF
