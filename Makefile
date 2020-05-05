fwrite: fwrite.c
	$(CC) -g -o fwrite fwrite.c

interpose_write.so: interpose_write.c
	$(CC) -g -shared -fPIC -o $@ $< -ldl

interpose_fwrite.so: interpose_fwrite.c
	$(CC) -g -shared -fPIC -o $@ $< -ldl
