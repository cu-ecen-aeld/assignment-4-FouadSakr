CC ?= gcc
CROSS_COMPILE ?=

CFLAGS = -Wall -Werror
LDFLAGS =

TARGET = aesdsocket
SOURCES = aesdsocket.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

default: all

clean:
	rm -f $(TARGET)

.PHONY: all default clean