OBJS=call_fwrite call_write fwrite interpose_fwrite.so interpose_underbar_write.so interpose_write.so

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

call_fwrite: call_fwrite.c
	$(CC) -g -o $@ $<

call_write: call_write.c
	$(CC) -g -o $@ $<

interpose_write.so: interpose_write.c
	$(CC) -g -shared -fPIC -o $@ $< -ldl

interpose_fwrite.so: interpose_fwrite.c
	$(CC) -g -shared -fPIC -o $@ $< -ldl

interpose_underbar_write.so: interpose_underbar_write.c
	$(CC) -g -shared -fPIC -o $@ $< -ldl
