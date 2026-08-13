CC = gcc
CFLAGS = -Wall -Iinclude

TESTER_SRC = \
src/tester.c \
src/isotp.c \
src/can_common.c

TARGET_SRC = \
src/target.c \
src/isotp.c \
src/uds.c \
src/can_common.c

tester:
	$(CC) $(TESTER_SRC) -o tester $(CFLAGS)

target:
	$(CC) $(TARGET_SRC) -o target $(CFLAGS)

all: tester target

clean:
	rm -f tester target
