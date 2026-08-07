CC = gcc
CFLAGS = -Wall -Iinclude

TESTER_SRC = \
src/tester.c \
src/can_common.c \
src/packet.c \
src/firmware_sender.c

TARGET_SRC = \
src/target.c \
src/can_common.c \
src/packet.c \
src/firmware_receiver.c \
src/crc.c

tester:
	$(CC) $(TESTER_SRC) -o tester $(CFLAGS)

target:
	$(CC) $(TARGET_SRC) -o target $(CFLAGS)

all: tester target

clean:
	rm -f tester target
