################################################################################
#              Makefile for the CS 3410 Cache Project 5                        #
################################################################################

# Additional flags for the compiler
# always enable debugging because its more convenient
CFLAGS := -std=c99 -D_GNU_SOURCE -Wall -g3
LFLAGS := -lm

.PHONY: all clean run

all: clean p5

p5: cache.o cache_stats.o simulator.o print_helpers.o
	gcc $(CFLAGS) -o $@ $@.c $^ $(LFLAGS)

# Wildcard rule that allows for the compilation of a *.c file to a *.o file
%.o : %.c
	gcc -c $(CFLAGS) $< -o $@

# Removes any executables and compiled object files
clean:
	rm -f p5 *.o
