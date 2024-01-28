SHELL := /bin/bash

objs = png.o
CC = g++

Q=@

CFLAGS = -g -O2 -std=c++17 -MMD -Wall -Wextra

CC = x86_64-w64-mingw32-g++
EXEC = lestpng.dll
LDFLAGS = -lzlib1 -lws2_32 -shared -static-libgcc -static-libstdc++

ifeq ($(BUILD), D)
	CFLAGS += -DDEBUG
endif

all: $(EXEC)

PngDeps := $(patsubst %.o, %.d, $(objs))
-include $(PngDeps)
export

%.o: %.cpp
	@echo "MAKE $<"
	$(Q)$(CC) $(CFLAGS) $< -c

help:
	@echo "Available commands are:"
	@echo "make TYPE=lib TARGET=win"
	@echo "make TARGET=win"
	@echo "make BUILD=D"

$(EXEC): $(objs)
	@echo "MAKE $@"
	$(Q)$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

clean:
	rm *.o *.d *.dll *.a
