CC=gcc

CFLAGS=-Wall

COMMON=can_common.c

all: tester target

tester:
	$(CC) tester.c $(COMMON) -o tester $(CFLAGS)

target:
	$(CC) target.c $(COMMON) -o target $(CFLAGS)

clean:
	rm -f tester target
