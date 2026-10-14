# RO-MP Dedicated Server — Makefile
#   make linux    -> binari Linux   (../bin/linux/romp-server)
#   make windows  -> binari Windows (../bin/windows/romp-server.exe)
#                   butuh x86_64-w64-mingw32-gcc (paket mingw-w64).
#                   Alternatif tanpa install: pakai Zig:
#                     zig cc -O2 -target x86_64-windows-gnu -o ../bin/windows/romp-server.exe romp_server.c -lws2_32 -pthread
#   make all      -> keduanya
#   make clean    -> hapus binari

CC_LINUX   = gcc
CC_WINDOWS = x86_64-w64-mingw32-gcc

CFLAGS = -O2 -Wall -Wextra -std=c99
SRC    = romp_server.c

LINUX_OUT   = ../bin/linux/romp-server
WINDOWS_OUT = ../bin/windows/romp-server.exe

all: linux windows

linux:
	mkdir -p ../bin/linux
	$(CC_LINUX) $(CFLAGS) -o $(LINUX_OUT) $(SRC) -pthread

windows:
	mkdir -p ../bin/windows
	$(CC_WINDOWS) $(CFLAGS) -o $(WINDOWS_OUT) $(SRC) -lws2_32 -pthread -static

clean:
	rm -f $(LINUX_OUT) $(WINDOWS_OUT)

.PHONY: all linux windows clean
