# ================= CONFIG =================
CC      := cc
CFLAGS  := -Wall -Wextra -O2
TARGET  := qbtctl
SRCS    := qbtctl.c auth.c help.c cJSON.c

# ================= BUILD =================
all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -lcurl -o $(TARGET)

# ================= CLEAN =================
clean:
	rm -f $(TARGET)
