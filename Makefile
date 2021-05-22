CC = gcc

CFLAGS = -g -O0 -fPIC -D_GNU_SOURCE -Wall -Wextra -Werror
LDFLAGS = -shared -ldl

all: bin/libfailingmalloc.so

bin/libfailingmalloc.so: failingmalloc.c
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)

bin/example-safe: example.c
	$(CC) $^ $(CFLAGS) -fno-builtin -DSAFE -o $@

bin/example-unsafe: example.c
	$(CC) $^ $(CFLAGS) -fno-builtin -o $@

clean:
	rm -f bin/*

check: bin/example-safe bin/example-unsafe
	@LD_PRELOAD=bin/libfailingmalloc.so bin/example-safe 123 456 && echo Safe test terminated normally
	@LD_PRELOAD=bin/libfailingmalloc.so bin/example-unsafe 1 2 3 || echo Unsafe test failed with $$?

.PHONY: all clean
