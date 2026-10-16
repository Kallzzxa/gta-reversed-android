/*
 * RO-MP override dunia per-pemain ala SA-MP — server.
 *
 * Alur ala SA-MP:
 *   1. Gamemode memanggil romp.setPlayerTime(pid, hour, minute) atau
 *      romp.setPlayerWeather(pid, weatherid). Server memvalidasi nilai,
 *      menyimpan override di Client, lalu mengirim frame 0x39 WORLD_SET
 *      ke pemain itu SAJA dengan nilai efektif (override bila ada,
 *      global bila tidak). Pemain lain tak dapat apa-apa — persis
 *      seperti SA-MP: SetPlayerTime/SetPlayerWeather hanya memengaruhi
 *      pemain yang dituju.
 *   2. romp.togglePlayerClock(pid, toggle) mengirim paket baru 0x54
 *      CLOCK_SET (u8 pid | u8 clock_enabled) ke pemain itu saja:
 *      1 = jam berjalan normal, 0 = jam dibekukan ala TogglePlayerClock.
 *   3. Perubahan dunia GLOBAL (romp.setGravity/setWeather/setWorldTime)
 *      tetap di-broadcast, tapi override-aware: pemain yang punya
 *      override menerima 0x39 efektif (override mereka dipertahankan),
 *      pemain lain menerima nilai global baru.
 *   4. Pemain yang baru join di-resync 0x39 efektif (via world_send_to)
 *      + 0x54 CLOCK_SET sesuai state jamnya.
 *
 * Gravity tetap global (SA-MP tak punya per-player gravity override).
 *
 * Tak ada event Lua: setter bersifat pasif seperti di SA-MP.
 *
 * NPC didukung: state disimpan, paket ke NPC sendiri no-op (tak punya
 * socket) — sama seperti modul per-pemain lain (skill/money).
 *
 * Protokol (lihat server/PROTOCOL.md):
 *   0x39 WORLD_SET (server -> client): TIDAK BERUBAH FORMAT — nilainya
 *     efektif per-pemain (override bila ada, global bila tidak).
 *   0x54 CLOCK_SET (server -> client):
 *     u8 pid | u8 clock_enabled     (3 byte frame, 2 byte payload)
 *     clock_enabled: 1 = jam berjalan, 0 = dibekukan.
 */
#pragma once

#include "romp_server.h"

/* Sentinel "ikut global" untuk field override di Client. */
#define PWORLD_FOLLOW_GLOBAL -1

/* Inisialisasi override untuk pemain/NPC baru (ikut global, jam jalan). */
void pworld_init(Client *c);

/* Bersihkan state saat pemain leave (kembalikan ikut global & jam jalan
 * agar tak bocor bila struct dipakai ulang). Tak mengirim paket apa pun
 * (P_LEAVE sudah memberitahu semua). */
void pworld_on_player_leave(Client *c);

/* Set override jam ala SA-MP SetPlayerTime.
 * Validasi hour 0..23, minute 0..59; kirim 0x39 efektif ke pemain tsb.
 * Kembalikan 1 bila berhasil; 0 bila pemain tak valid / nilai tak valid
 * (state tak berubah, tak ada paket). */
int pworld_set_time(Client *c, int hour, int minute);

/* Ambil jam EFEKTIF (override bila ada, global bila tidak) + flag apakah
 * memakai override. Kembalikan 0 bila pemain tak valid. */
int pworld_get_time(const Client *c, int *hour, int *minute, int *is_override);

/* Set override cuaca ala SA-MP SetPlayerWeather.
 * Validasi weather 0..255; kirim 0x39 efektif ke pemain tsb.
 * Kembalikan 1/0 seperti pworld_set_time. */
int pworld_set_weather(Client *c, int weather);

/* Ambil cuaca EFEKTIF + flag override. Kembalikan 0 bila tak valid. */
int pworld_get_weather(const Client *c, int *weather, int *is_override);

/* Toggle jam ala SA-MP TogglePlayerClock.
 * on=1: jam berjalan; on=0: dibekukan. Kirim 0x54 ke pemain tsb.
 * Kembalikan 1 bila berhasil; 0 bila pemain tak valid (tanpa paket). */
int pworld_toggle_clock(Client *c, int on);

/* 1 = jam berjalan, 0 = dibekukan, -1 = pemain tak valid. */
int pworld_get_clock(const Client *c);

/* Kirim 0x39 efektif ke satu client (dipakai pworld_set_*;
 * world_send_to sendiri sudah override-aware). */
int pworld_send_effective(Client *c);

/* Resync state jam ke pemain yang baru join (kirim 0x54).
 * 0x39 efektif sudah dikirim world_send_to di do_join. */
void pworld_sync_join(Client *c);
