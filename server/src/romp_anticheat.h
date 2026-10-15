/*
 * RO-MP anti-cheat dasar ala SA-MP.
 *
 * Dipakai oleh core server (romp_server.c) untuk mendeteksi cheat yang
 * paling umum:
 *   - teleport     : lompatan posisi mendadak (>= AC_TELEPORT_DIST unit)
 *   - speed hack   : kecepatan gerak melebihi batas wajar pemain/kendaraan
 *   - health hack  : laporan health client melebihi nilai otoritatif server
 *   - armour hack  : laporan armour client melebihi nilai otoritatif server
 *
 * Health/armour adalah otoritatif server: gamemode yang mengubahnya lewat
 * API Lua romp.setPlayerHealth / romp.setPlayerArmour (health hack dihitung
 * dari nilai server, bukan dari terakhir kali client lapor).
 *
 * Setiap temuan menambah 1 peringatan untuk pemain, diteruskan ke event Lua
 * onPlayerCheat(playerid, type, value), dan diumumkan ke semua pemain.
 * Kalau peringatan mencapai ac_maxwarnings (config server.cfg, 0 = tidak
 * pernah kick otomatis), pemain otomatis di-kick.
 *
 * Batasan v1: batas kecepatan memakai angka tetap; sinkronisasi kendaraan
 * belum diperiksa (sopir menyetir via paket 0x12, batas kendaraan hanya
 * dipakai untuk posisi pemainnya).
 */
#pragma once

#include <stdint.h>

/* Ambang deteksi (satuan dunia RO-MP ~= meter ala GTA:SA). */
#define AC_TELEPORT_DIST  150.0f  /* lompatan >= ini = teleport */
#define AC_SPEED_FOOT     60.0f   /* unit/detik, jalan kaki/lari (sprint SA ~12) */
#define AC_SPEED_VEH      250.0f  /* unit/detik, di kendaraan (pesawat SA ~75) */
#define AC_STAT_TOL       5.0f    /* toleransi laporan health/armour client */
#define AC_STAT_GRACE_MS  1500    /* abaikan laporan 1.5 dtk setelah server set */
#define AC_POS_GRACE_MS   2000    /* abaikan cek teleport 2 dtk setelah
                                   * server memindahkan pemain
                                   * (romp.setPlayerPos) */

struct Client;

/* Cek paket 0x10 POS sebelum posisi disimpan. Dipanggil setelah rate-limit
 * 33 Hz, memakai c->last_pos_ms & c->pos_x/y/z yang lama.
 * Return 1 = paket dibuang (teleport, atau paket basi dalam grace
 * setPlayerPos — yang terakhir dibuang diam-diam tanpa peringatan),
 * 0 = lanjutkan normal. */
int ac_check_movement(struct Client *c, float nx, float ny, float nz,
                      uint64_t now);

/* Cek paket 0x26 STAT (health/armour yang dilaporkan client). */
void ac_check_stat(struct Client *c, uint16_t health, uint16_t armour,
                   uint64_t now);

/* Set otoritatif dari scripting (romp.setPlayerHealth/setPlayerArmour).
 * Nilai di-clamp 0..100 dan laporan client diabaikan selama AC_STAT_GRACE_MS. */
void ac_set_health(struct Client *c, float hp, uint64_t now);
void ac_set_armour(struct Client *c, float ap, uint64_t now);

/* Lapor temuan cheat: +1 peringatan, event Lua onPlayerCheat, pengumuman,
 * dan auto-kick bila mencapai ac_maxwarnings. Dipakai core & modul lain. */
void ac_report(struct Client *c, const char *type, double value);
