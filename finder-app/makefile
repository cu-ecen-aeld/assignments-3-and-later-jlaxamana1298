CC := $(CROSS_COMPILE)gcc
TARGET := writer

all: $(TARGET)

$(TARGET): writer.c
	$(CC) -o $(TARGET) writer.c

clean:
	rm -f $(TARGET) writer.o
