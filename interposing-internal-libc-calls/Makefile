CFLAGS=-g -Wall -Werror
LDLIBS=-ldl

OBJS=call_fwrite call_write interpose_fwrite.so interpose_underbar_write.so interpose_write.so

all: gitignore README.md

README.md: README.ipynb $(OBJS)

include ../common/Makefile.common
