PROJECT := MiniFTP

AMIGA_PREFIX ?= /opt/amiga
NETINCLUDE_DIR ?= /opt/amiga-netinclude

CROSS := $(AMIGA_PREFIX)/bin/m68k-amigaos-
CC := $(CROSS)gcc

CPPFLAGS := -Iinclude -I$(NETINCLUDE_DIR)/include
CFLAGS := -O2 -Wall -Wextra -mcrt=nix13 -DAMITCP13_OS13

SRCS = src/MiniFTP.c
OBJS = $(SRCS:.c=.o)

all: build/MiniFTP

build:
	mkdir -p build

build/MiniFTP: build $(OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(OBJS)

clean:
	rm -f $(OBJS) build/MiniFTP

.PHONY: all clean
