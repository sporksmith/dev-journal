CFLAGS=-g -Wall -Werror
LDLIBS=-ldl
OBJS=call_write libinterpose.so

all: gitignore README.md

README.md: README.md.sh glibc-build/libc.so $(OBJS)
	./README.md.sh > README.md

glibc-build/libc.so:
	git submodule init -- glibc
	git submodule update -- glibc
	mkdir -p glibc-build
	cd glibc-build && ../glibc/configure --disable-sanity-checks
	cd glibc-build && make -j8

include ../common/Makefile.common
