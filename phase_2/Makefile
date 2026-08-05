CC = gcc

CFLAGS = -Wall -Iinclude

TESTER_SRC = \
	src/tester.c \
	src/firmware_sender.c \
	src/packet.c

tester:
	$(CC) $(TESTER_SRC) -o tester $(CFLAGS)

clean:
	rm -f tester
