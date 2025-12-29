CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread
SRC_DIR = src
TARGET = server

SRCS = $(SRC_DIR)/server.c $(SRC_DIR)/queue.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

clean:
	rm -f $(TARGET) $(OBJS)
