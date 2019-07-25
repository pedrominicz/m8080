INCLUDES = -I..
CFLAGS = -pedantic -Wall -O3
LDFLAGS = -lallegro

all: debug disassembler invaders tests

debug: debug.c
	$(CC) $(INCLUDES) $(CFLAGS) $< -o $@

disassembler: disassembler.c
	$(CC) $(INCLUDES) $(CFLAGS) $< -o $@

invaders: invaders.c
	$(CC) $(INCLUDES) $(CFLAGS) $< -o $@ $(LDFLAGS)

tests: tests.c
	$(CC) $(INCLUDES) $(CFLAGS) $< -o $@

clean:
	rm -f debug disassembler invaders tests

.PHONY: all clean
