# Makefile for qbtctl project
# -----------------------------
# Usage:
#   make         -> build dynamically linked binary
#   make static  -> build statically linked binary (output: qbtctl)
#   make clean   -> remove build artifacts
#   make install -> install binary to /usr/local/bin

# Compiler and flags
CC      := gcc
CFLAGS  := -O2 -Wall -Wextra
LDFLAGS := -lcurl -lsodium -lz -lm -ldl -lpthread

# Source files
SRCS    := qbtctl.c watch.c cJSON.c auth.c help.c
OBJS    := $(SRCS:.c=.o)

# Output binary
TARGET := qbtctl

# Installation directory
PREFIX  := /usr/local
BINDIR  := $(PREFIX)/bin

# Default target: dynamic build
all: $(TARGET)

# Link dynamically
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Build statically
static: $(OBJS)
	$(CC) $(CFLAGS) -static -o $(TARGET) $^ $(LDFLAGS)

# Compile each C file to object file
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	@echo "Cleaning..."
	rm -f $(OBJS) $(TARGET)

# Install binary
install: $(TARGET)
	@echo "Installing $(TARGET) to $(BINDIR)..."
	@if [ -w $(BINDIR) ] || [ ! -e $(BINDIR)/$(TARGET) ]; then \
		install -Dm755 $(TARGET) $(BINDIR)/$(TARGET); \
		if [ -f $(BINDIR)/$(TARGET) ]; then \
			echo "Installed $(BINDIR)/$(TARGET) successfully"; \
		else \
			echo "Error: Installation failed, try 'sudo make install'"; \
			exit 1; \
		fi \
	else \
		echo "Error: No write permission to $(BINDIR), try 'sudo make install'"; \
		exit 1; \
	fi

# Phony targets
.PHONY: all static clean install
