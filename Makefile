CC = gcc
CFLAGS = -Wall
SRC = src/ls-v1.0.0.c
TARGET = bin/ls

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
