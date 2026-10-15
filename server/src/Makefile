# RO-MP Dedicated Server — Makefile (sekarang dengan Lua scripting)
#   make linux    -> binari Linux   (../bin/linux/romp-server)
#   make windows  -> binari Windows (../bin/windows/romp-server.exe)
#   make all      -> keduanya
#   make clean    -> hapus binari
#
# Windows butuh x86_64-w64-mingw32-gcc (paket mingw-w64).
# Alternatif tanpa install (yang dipakai untuk build rilis):
#   cd server/src && ~/workspace/toolchain/zig-x86_64-linux-0.15.2/zig cc -O2 -target x86_64-windows-gnu \
#     -o ../bin/windows/romp-server.exe romp_server.c romp_lua.c romp_vehicle.c romp_label.c romp_weapon.c \
#     romp_anticheat.c romp_sqlite.c romp_timer.c romp_object.c romp_actor.c romp_audio.c romp_world.c romp_attach.c romp_skill.c romp_marker.c romp_death.c ../vendor/lua/*.c ../vendor/sqlite/sqlite3.c \
#     -I../vendor/lua -I../vendor/sqlite -lws2_32 -pthread
#   ~/workspace/toolchain/zig-x86_64-linux-0.15.2/zig cc -O2 -target x86_64-windows-gnu \
#     -o ../bin/windows/romp-master.exe romp_master.c -lws2_32

CC_LINUX   = gcc
CC_WINDOWS = x86_64-w64-mingw32-gcc

CFLAGS   = -O2 -Wall -Wextra
SRC      = romp_server.c romp_lua.c romp_vehicle.c romp_label.c romp_weapon.c romp_anticheat.c romp_sqlite.c romp_timer.c romp_object.c romp_actor.c romp_audio.c romp_world.c romp_attach.c romp_skill.c romp_marker.c romp_death.c romp_wanted.c romp_money.c
LUA_SRC  = $(wildcard ../vendor/lua/*.c)
LUA_INC  = -I../vendor/lua
# SQLite amalgamation (vendor): dikompilasi terpisah tanpa -Wall agar
# warning internal SQLite tidak mengotori build kode RO-MP.
SQLITE_SRC = ../vendor/sqlite/sqlite3.c
SQLITE_INC = -I../vendor/sqlite
SQLITE_OBJ = obj_sqlite3.o

LINUX_OUT   = ../bin/linux/romp-server
WINDOWS_OUT = ../bin/windows/romp-server.exe
MASTER_SRC  = romp_master.c
MASTER_LINUX_OUT   = ../bin/linux/romp-master
MASTER_WINDOWS_OUT = ../bin/windows/romp-master.exe

all: linux windows master

linux:
	mkdir -p ../bin/linux
	$(CC_LINUX) -O2 -c $(SQLITE_SRC) $(SQLITE_INC) -o ../bin/linux/$(SQLITE_OBJ)
	$(CC_LINUX) $(CFLAGS) -o $(LINUX_OUT) $(SRC) ../bin/linux/$(SQLITE_OBJ) $(LUA_SRC) $(LUA_INC) $(SQLITE_INC) -pthread -lm -ldl

windows:
	mkdir -p ../bin/windows
	$(CC_WINDOWS) -O2 -c $(SQLITE_SRC) $(SQLITE_INC) -o ../bin/windows/$(SQLITE_OBJ)
	$(CC_WINDOWS) $(CFLAGS) -o $(WINDOWS_OUT) $(SRC) ../bin/windows/$(SQLITE_OBJ) $(LUA_SRC) $(LUA_INC) $(SQLITE_INC) -lws2_32 -pthread

master:
	mkdir -p ../bin/linux
	$(CC_LINUX) $(CFLAGS) -o $(MASTER_LINUX_OUT) $(MASTER_SRC) -pthread
	@echo "master linux OK (windows: build manual via zig, lihat komentar di atas)"

clean:
	rm -f $(LINUX_OUT) $(WINDOWS_OUT) $(MASTER_LINUX_OUT) $(MASTER_WINDOWS_OUT)
	rm -f ../bin/linux/$(SQLITE_OBJ) ../bin/windows/$(SQLITE_OBJ)

.PHONY: all linux windows master clean
