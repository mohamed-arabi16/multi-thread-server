CC = gcc
CFLAGS = -Wall -Wextra -O2
SRC_DIR = src
TARGET = server

SRCS = $(SRC_DIR)/server.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

clean:
	rm -f $(TARGET) $(OBJS)
