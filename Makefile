CC = gcc
CFLAGS = -Wall -Wextra -Werror -O2 -pthread
SRC_DIR = src
TARGET = server

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC_DIR)/server.c
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC_DIR)/server.c

clean:
	rm -f $(TARGET)
