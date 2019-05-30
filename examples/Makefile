INCLUDES = -I..
CFLAGS = -pedantic -Wall -O3
LDFLAGS = -lallegro

invaders: invaders.c
	$(CC) $(INCLUDES) $(CFLAGS) $< -o $@ $(LDFLAGS)

tests: tests.c
	$(CC) $(INCLUDES) $(CFLAGS) $< -o $@

all: invaders tests

.PHONY: all invaders tests
