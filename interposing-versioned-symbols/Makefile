CFLAGS=-g -Wall -Werror
LDLIBS=-ldl
LDFLAGS=-L .

all: README.md

README.md: README.md.sh user_v0 user_v1 user_v2 user_v3 libinterposer.so libinterposer_v1v2.so libinterposer_v1v2_default.so
	./README.md.sh > README.md

lib%.so: %.c %.map
	$(CC) -shared -fPIC $(CFLAGS) -o $@ $< -Wl,--version-script=$*.map $(LDFLAGS) $(LDLIBS)

lib%.so: %.c
	$(CC) -shared -fPIC $(CFLAGS) -o $@ $< $(LDFLAGS) $(LDLIBS)

user_v0: user_v1.c libtarget_v0.so
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< -ltarget_v0

user_v1: user_v1.c libtarget_v1.so
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< -ltarget_v1

user_v2: user_v2.c libtarget_v2.so
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< -ltarget_v2

user_v3: user_v2.c libtarget_v3.so
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< -ltarget_v3
