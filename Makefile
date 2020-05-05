README.md: Interposing\ internal\ libc\ calls.ipynb fwrite interpose_write.so interpose_fwrite.so interpose_underbar_write.so
	jupyter nbconvert Interposing\ internal\ libc\ calls.ipynb --execute --to markdown --output=$@

fwrite: fwrite.c
	$(CC) -g -o fwrite fwrite.c

interpose_write.so: interpose_write.c
	$(CC) -g -shared -fPIC -o $@ $< -ldl

interpose_fwrite.so: interpose_fwrite.c
	$(CC) -g -shared -fPIC -o $@ $< -ldl

interpose_underbar_write.so: interpose_underbar_write.c
	$(CC) -g -shared -fPIC -o $@ $< -ldl
