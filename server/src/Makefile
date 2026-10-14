# RO-MP Dedicated Server — Makefile (sekarang dengan Lua scripting)
#   make linux    -> binari Linux   (../bin/linux/romp-server)
#   make windows  -> binari Windows (../bin/windows/romp-server.exe)
#   make all      -> keduanya
#   make clean    -> hapus binari
#
# Windows butuh x86_64-w64-mingw32-gcc (paket mingw-w64).
# Alternatif tanpa install (yang dipakai untuk build rilis):
#   ~/workspace/toolchain/zig-x86_64-linux-0.15.2/zig cc -O2 -target x86_64-windows-gnu \
#     -o ../bin/windows/romp-server.exe romp_server.c romp_lua.c vendor/lua/*.c \
#     -Ivendor/lua -lws2_32 -pthread

CC_LINUX   = gcc
CC_WINDOWS = x86_64-w64-mingw32-gcc

CFLAGS   = -O2 -Wall -Wextra
SRC      = romp_server.c romp_lua.c
LUA_SRC  = $(wildcard ../vendor/lua/*.c)
LUA_INC  = -I../vendor/lua

LINUX_OUT   = ../bin/linux/romp-server
WINDOWS_OUT = ../bin/windows/romp-server.exe

all: linux windows

linux:
	mkdir -p ../bin/linux
	$(CC_LINUX) $(CFLAGS) -o $(LINUX_OUT) $(SRC) $(LUA_SRC) $(LUA_INC) -pthread -lm -ldl

windows:
	mkdir -p ../bin/windows
	$(CC_WINDOWS) $(CFLAGS) -o $(WINDOWS_OUT) $(SRC) $(LUA_SRC) $(LUA_INC) -lws2_32 -pthread

clean:
	rm -f $(LINUX_OUT) $(WINDOWS_OUT)

.PHONY: all linux windows clean
