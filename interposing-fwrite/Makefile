README.md: README.ipynb call_fwrite interpose_write.so interpose_fwrite.so interpose_underbar_write.so
	jupyter nbconvert --execute --to markdown --stdout $< | ansi2txt | awk -f prefix.awk > $@

call_fwrite: call_fwrite.c
	$(CC) -g -o $@ $<

interpose_write.so: interpose_write.c
	$(CC) -g -shared -fPIC -o $@ $< -ldl

interpose_fwrite.so: interpose_fwrite.c
	$(CC) -g -shared -fPIC -o $@ $< -ldl

interpose_underbar_write.so: interpose_underbar_write.c
	$(CC) -g -shared -fPIC -o $@ $< -ldl
