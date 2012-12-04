all: libbobcat.so.1.0

libbobcat.so.1.0: seqfile.o
	gcc -D_FILE_OFFSET_BITS=64 -D_LARGE_FILE -shared -Wl,-soname,libbobcat.so.1 -o $@ $^ -lsqlite3
seqfile.o: seqfile.c seqfile.h
	gcc -D_FILE_OFFSET_BITS=64 -D_LARGE_FILE -c -fPIC seqfile.c -lsqlite3
clean:
	-rm *.o *.so.*

.PHONY: clean