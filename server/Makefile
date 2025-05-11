CC = $(CROSS_COMPILE)gcc
CFLAGS = -pthread -Wall -Werror -g
TARGET = aesdsocket

all: aesdsocket

$(TARGET): aesdsocket.c
	$(CC) $(CFLAGS) -o $(TARGET) aesdsocket.c

clean:
	rm -f $(TARGET)

.PHONY: all clean
