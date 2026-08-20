CC = gcc
CFLAGS = -Wall -Wextra -Iinclude

COMMON_SRC = \
src/isotp.c \
src/can_common.c \
src/crc.c \
src/firmware_sender.c \
src/firmware_receiver.c \
src/uds.c \


TESTER_SRC = \
src/tester.c \
$(COMMON_SRC)

TARGET_SRC = \
src/target.c \
$(COMMON_SRC)

tester:
	$(CC) $(CFLAGS) $(TESTER_SRC) -o tester

target:
	$(CC) $(CFLAGS) $(TARGET_SRC) -o target

all: tester target

clean:
	rm -f tester target
