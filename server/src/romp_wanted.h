/*
 * RO-MP wanted level ala SA-MP (SetPlayerWantedLevel / GetPlayerWantedLevel)
 * — server.
 *
 * Alur ala SA-MP:
 *   1. Gamemode memanggil romp.setPlayerWantedLevel(pid, level) — mis.
 *      karena pemain melakukan kejahatan. Server memvalidasi level 0..6
 *      (ala SA-MP, 6 = paling dicari), menyimpan state otoritatif di
 *      Client.wanted_level, lalu broadcast 0x48 WANTED_SET ke SEMUA
 *      pemain yang connect (wanted level bersifat global — semua orang
 *      bisa melihat bintang wanted pemain lain, seperti di SA-MP).
 *   2. Perubahan menyalakan event Lua onPlayerWantedLevelChange(pid,
 *      oldlevel, newlevel) — perpanjangan RO-MP (SA-MP tak punya event
 *      ini) agar gamemode bisa menjalankan logika polisi: kirim mobil
 *      patroli (cop car), ganti skin polisi, dsb.
 *   3. Pemain yang baru join di-resync wanted semua pemain yang connect
 *      (wanted_sync_join), termasuk levelnya sendiri (default 0).
 *
 * Level 0 = bersih (tak dicari polisi). NPC didukung: state + broadcast
 * jalan (paket ke NPC sendiri no-op karena tak punya socket).
 *
 * Protokol (lihat server/PROTOCOL.md):
 *   0x48 WANTED_SET (server -> semua client):
 *     u8 pid | u8 level             (3 byte frame, 2 byte payload)
 *     level 0..6.
 */
#pragma once

#include "romp_server.h"

/* Level wanted ala SA-MP: 0 = bersih .. 6 = paling dicari. */
#define WANTED_LEVEL_MIN 0
#define WANTED_LEVEL_MAX 6

/* 1 = level 0..6 valid, 0 = tidak. */
int wanted_valid_level(int level);

/* Set wanted level pemain ala SA-MP SetPlayerWantedLevel.
 * Simpan state otoritatif, broadcast 0x48 ke semua pemain, nyalakan event
 * Lua onPlayerWantedLevelChange(pid, old, new).
 * Kembalikan 1 bila berhasil; 0 bila pemain tak valid / level di luar 0..6
 * (tanpa broadcast, tanpa event, state tak berubah). */
int wanted_set(Client *c, int level);

/* Get wanted level ala SA-MP GetPlayerWantedLevel; -1 bila pemain tak valid. */
int wanted_get(const Client *c);

/* Inisialisasi wanted level (0) untuk pemain baru / NPC baru. */
void wanted_init(Client *c);

/* Bersihkan state saat pemain leave (kembalikan 0 agar tak bocor bila
 * struct dipakai ulang). Tidak mengirim paket apa pun. */
void wanted_on_player_leave(Client *c);

/* Resync wanted level semua pemain yang connect ke pemain yang baru join
 * (termasuk miliknya sendiri). */
void wanted_sync_join(Client *c);
