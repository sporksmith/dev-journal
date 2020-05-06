README.md: README.md.sh call_write libinterpose.so glibc-build/libc.so
	./README.md.sh > README.md

glibc-build/libc.so:
	git submodule init -- glibc
	git submodule update -- glibc
	mkdir -p glibc-build
	cd glibc-build && ../glibc/configure --disable-sanity-checks
	cd glibc-build && make -j8

call_write: call_write.c
	gcc -g -o call_write call_write.c

libinterpose.so: interpose.c
	gcc -fPIC -g -L. -shared -o libinterpose.so interpose.c

libexplicit_interpose.so: explicit_interpose.c
	gcc -fPIC -g -L. -shared -o libexplicit_interpose.so explicit_interpose.c -ldl
