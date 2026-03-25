# =======================
# qbtctl Makefile
# =======================

CC      := gcc
CFLAGS  := -O2 -Wall -Wextra
LDFLAGS := -lcurl -lpthread -lm -lsodium

SRCS    := qbtctl.c watch.c auth.c help.c cJSON.c
OBJS    := $(SRCS:.c=.o)
TARGET  := qbtctl

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
