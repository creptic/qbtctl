CC      := gcc
CFLAGS  := -O2 -Wall -Wextra
LDFLAGS := -lm -ldl -lpthread

QBTCTL_SRC := auth.c cJSON.c help.c qbtctl.c
QBTCTL_BIN := qbtctl

# Default: dynamic
LIBS := -lcurl -lsodium -lz -lmbedtls -lmbedx509 -lmbedcrypto

# Static build toggle
ifeq ($(STATIC),1)
    CFLAGS  += -static
    LDFLAGS += -static
endif

all: $(QBTCTL_BIN)

$(QBTCTL_BIN):
	$(CC) $(CFLAGS) $(QBTCTL_SRC) -o $(QBTCTL_BIN) \
	$(LIBS) $(LDFLAGS)

clean:
	rm -f $(QBTCTL_BIN)

.PHONY: all clean
