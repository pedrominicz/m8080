INCLUDES = -I..
CFLAGS = -pedantic -Wall -O3
LDFLAGS = -lallegro

all: invaders tests

invaders: invaders.c
	$(CC) $(INCLUDES) $(CFLAGS) $< -o $@ $(LDFLAGS)

tests: tests.c
	$(CC) $(INCLUDES) $(CFLAGS) $< -o $@

clean:
	rm -f invaders tests

.PHONY: all clean
