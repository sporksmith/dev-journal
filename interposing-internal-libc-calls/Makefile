CFLAGS=-g -Wall -Werror
LDLIBS=-ldl
%.so: %.c
	$(CC) -shared -fPIC $(CFLAGS) -o $@ $< $(LDFLAGS) $(LDLIBS)

OBJS=call_fwrite call_write interpose_fwrite.so interpose_underbar_write.so interpose_write.so

all: gitignore README.md

.PHONY: gitignore
gitignore:
	rm -f .gitignore
	echo $(OBJS) | xargs -n1 >> .gitignore

.PHONY: clean
clean:
	rm -f $(OBJS)

README.md: README.ipynb $(OBJS)
	jupyter nbconvert --execute --to markdown --stdout $< | ansi2txt | awk -f prefix.awk > $@

