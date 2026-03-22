# Simple Makefile for qbtctl (dynamic linking)

CC := gcc
CFLAGS := -O2 -Wall -Wextra -Wno-unused-function
LDFLAGS := -lcurl -lsodium

SRC := $(wildcard *.c)
OBJ := $(SRC:.c=.o)
TARGET := qbtctl

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
