#!/bin/bash

show_cmd () {
  echo "    \$ $1"
  bash -c "$1" | sed 's/^/    /'
}

cat <<EOF
# Interposing versioned symbols

Build commands omitted, but can be seen in the accompanying \`Makefile\`. The
main thing to know is that where a \`.map\` file is specified, we use it by
passing \`-Wl,--version-script=foo.map\` in the link step.

## Setup

Suppose we have a program \`user_v1\`, compiled against v1 of a library
\`libtarget_v1.so\`:

Executable source:

`show_cmd "cat ./user_v1.c"`

Library source:

`show_cmd "cat ./target_v1.c"`

Library version map:

`show_cmd "cat ./target_v1.map"`

It'll be linked against v1 of \`libtarget\`'s symbols:

`show_cmd "readelf -s libtarget_v1.so | grep target"`

As expected, when we run it, v1 of the \`target\` function gets called:

`show_cmd "LD_LIBRARY_PATH=. ./user_v1"`

What happens if we try to interpose \`target\` via \`LD_PRELOAD\`? 

## Interposing with an unversioned library

It turns out we can still interpose \`target\` normally without using symbol
versioning at all.

Interposer source:

`show_cmd "cat ./interposer.c"`

We compile the interposer library without a version map, giving unversioned symbols:

`show_cmd "readelf -s libinterposer.so | grep target"`

When we use the \`libinterposer.so\` with \`LD_PRELOAD\`, our unversioned
symbol still overrides the versioned symbol:

`show_cmd "LD_LIBRARY_PATH=. LD_PRELOAD=./libinterposer.so ./user_v1"`

## Interposing specific versions

Suppose the API of \`target\` in \`libtarget\` changed in \`v2\`, and now
returns an integer:

`show_cmd "cat ./target_v2.c"`

And now suppose we have an updated version of the \`user\` program that uses
the newer version:

`show_cmd "cat ./user_v2.c"`

When run normally, it gets some expected return value:

`show_cmd "LD_LIBRARY_PATH=. ./user_v2"`

We can still use our old interposer library, but since it implements the old
API with a void return value, the caller will get garbage back:

`show_cmd "LD_LIBRARY_PATH=. LD_PRELOAD=./libinterposer.so ./user_v2"`

We could update our library to the new API, but suppose we want it work both
with programs using the older API and the newer API? It turns out we can use
the \`symver\` instruction to include multiple versions. In the source file we
give each implementation a unique name, and then use \`symver\` to remap those
names to multiple versions of the original symbol:

`show_cmd "cat ./interposer_v1v2.c"`

Resulting in a library with multiple versions of the symbol:

`show_cmd "readelf -s libinterposer_v1v2.so | grep target"`

When we run against v1 of the user program, we interpose with our v1 method:

`show_cmd "LD_LIBRARY_PATH=. LD_PRELOAD=./libinterposer_v1v2.so ./user_v1"`

When we run against v2 of the user program, we interpose with our v2 method:

`show_cmd "LD_LIBRARY_PATH=. LD_PRELOAD=./libinterposer_v1v2.so ./user_v2"`

## Unhandled versions

What happens, though, when there's later a v3 of \`libtarget\`?

`show_cmd "cat ./target_v3.c"`

`show_cmd "cat ./target_v3.map"`

If we try to use our multi-version interposer library, *neither* of our
implementations match, and the original function gets called.

`show_cmd "LD_LIBRARY_PATH=. LD_PRELOAD=./libinterposer_v1v2.so ./user_v3"`

We can address this by *also* providing an unversioned symbol. This will get
used if no specific symbol matches. We probably want to enumerate all the old
versions we know about, and then provide a default implementing the new API so
that it'll work with the future versions:

`show_cmd "cat ./interposer_v1v2_default.c"`

Even though we still didn't explicitly add a v3 symbol, it now works with v3:

`show_cmd "LD_LIBRARY_PATH=. LD_PRELOAD=./libinterposer_v1v2_default.so ./user_v3"`

EOF
