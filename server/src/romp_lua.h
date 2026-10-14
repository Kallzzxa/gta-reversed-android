/*
 * RO-MP scripting layer — gamemode pakai Lua (ala FiveM).
 *
 * Server core (romp_server.c) memanggil fungsi lua_on_* di bawah saat event
 * terjadi. Kalau gamemode Lua mengembalikan true (= handled), penanganan
 * default di C dilewati; kalau false/nil/tidak ada fungsinya, C pakai default.
 */
#pragma once

/* Muat & jalankan file gamemode. 0 = sukses, -1 = gagal (server tetap jalan tanpa Lua). */
int  lua_init(const char *script_path);
void lua_shutdown(void);
int  lua_active(void);

/* Event gamemode. Kembalikan 1 = handled (lewati default C), 0 = pakai default. */
int  lua_on_player_join(int id, const char *name);
int  lua_on_player_leave(int id, const char *name, int reason);
int  lua_on_player_chat(int id, const char *name, const char *text);
int  lua_on_player_command(int id, const char *name, const char *cmd, const char *args);
/* Dipanggil tiap 1 detik. */
void lua_on_tick(void);
