CFLAGS=-g -Wall -Werror
LDLIBS=-ldl
lib%.so : %.c
	$(CC) -shared -fPIC $(CFLAGS) -o $@ $< $(LDFLAGS) $(LDLIBS)

OBJS=call_write libinterpose.so

all: gitignore README.md

.PHONY: gitignore
gitignore:
	rm -f .gitignore
	echo $(OBJS) | xargs -n1 >> .gitignore

.PHONY: clean
clean:
	rm -f $(OBJS)

README.md: README.md.sh glibc-build/libc.so $(OBJS)
	./README.md.sh > README.md

glibc-build/libc.so:
	git submodule init -- glibc
	git submodule update -- glibc
	mkdir -p glibc-build
	cd glibc-build && ../glibc/configure --disable-sanity-checks
	cd glibc-build && make -j8
