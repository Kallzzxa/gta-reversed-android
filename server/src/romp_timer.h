/*
 * RO-MP timer Lua ala SA-MP SetTimer/SetTimerEx/KillTimer.
 *
 * Tiap script (gamemode + tiap filterscript) punya timer sendiri-sendiri:
 * timer yang dibuat dari state Lua A hanya memanggil callback di state A.
 * Semua pemanggilan hanya dari main thread server; timer_tick() dipanggil
 * tiap iterasi loop utama (~100 ms mengikuti select timeout).
 */
#pragma once

#include <stdint.h>

struct lua_State;

/* API Lua (didaftarkan sebagai romp.setTimer / killTimer / getTimerCount). */
int l_romp_set_timer(struct lua_State *L);
int l_romp_kill_timer(struct lua_State *L);
int l_romp_get_timer_count(struct lua_State *L);

/* Tembak semua timer yang sudah jatuh tempo pada waktu `now` (ms monotonik). */
void timer_tick(uint64_t now);

/* Hapus semua timer milik state Lua L (dipanggil SEBELUM lua_close saat
 * filterscript di-unload atau gamemode dimatikan). */
void timer_kill_state(struct lua_State *L);
