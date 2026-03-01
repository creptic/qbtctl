# ================= CONFIG =================
CC      := cc
CFLAGS  := -Wall -Wextra -O2
TARGET  := qbtctl
SRCS    := qbtctl.c auth.c help.c cJSON.c

# ================= PKG-CONFIG CHECK =================
CURL_FLAGS := $(shell pkg-config --cflags --libs libcurl 2>/dev/null)

# ================= BUILD =================
all: $(TARGET)

$(TARGET): $(SRCS)
ifeq ($(CURL_FLAGS),)
	$(error "libcurl not found! Please install libcurl and pkg-config.")
endif
	$(CC) $(CFLAGS) $(SRCS) $(CURL_FLAGS) -o $(TARGET)

# ================= CLEAN =================
clean:
	rm -f $(TARGET)
