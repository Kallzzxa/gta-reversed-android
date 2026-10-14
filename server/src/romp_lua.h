/*
 * RO-MP scripting layer — gamemode pakai Lua (ala FiveM).
 *
 * Server core (romp_server.c) memanggil fungsi lua_on_* di bawah saat event
 * terjadi. Kalau gamemode Lua mengembalikan true (= handled), penanganan
 * default di C dilewati; kalau false/nil/tidak ada fungsinya, C pakai default.
 */
#pragma once

#include <stddef.h> /* size_t untuk lua_on_player_request */

/* Muat & jalankan file gamemode. 0 = sukses, -1 = gagal (server tetap jalan tanpa Lua). */
int  lua_init(const char *script_path);
void lua_shutdown(void);
int  lua_active(void);

/* Event gamemode. Kembalikan 1 = handled (lewati default C), 0 = pakai default. */
int  lua_on_player_join(int id, const char *name);
int  lua_on_player_leave(int id, const char *name, int reason);
int  lua_on_player_chat(int id, const char *name, const char *text);
int  lua_on_player_command(int id, const char *name, const char *cmd, const char *args);
/* Callback NUI dari client (selalu dianggap handled, tak ada default C). */
int  lua_on_nui_callback(int id, const char *name, const char *data);
/* Event console: true = di-handle gamemode. */
int  lua_on_console_command(const char *cmd, const char *args);
/* Event pre-connect: true = tolak (reason_out diisi alasan). */
int  lua_on_player_request(const char *name, const char *ip,
                           char *reason_out, size_t reason_sz);
/* Event kendaraan ala SA-MP (tidak ada default C; gamemode opsional). */
void lua_on_player_enter_vehicle(int pid, int vid, int seat);
void lua_on_player_exit_vehicle(int pid, int vid);
void lua_on_vehicle_damage(int vid, int damagerid, int newhealth);
void lua_on_vehicle_death(int vid, int killerid);
void lua_on_vehicle_spawn(int vid);
/* Dipanggil tiap 1 detik. */
void lua_on_tick(void);
