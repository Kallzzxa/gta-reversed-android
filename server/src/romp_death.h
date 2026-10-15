/*
 * RO-MP kill feed ala SA-MP (SendDeathMessage / OnPlayerDeath) — server.
 *
 * Alur ala SA-MP:
 *   1. Client yang mati mengirim 0x46 DEATH_REPORT (u8 killerid, u8 reason);
 *      victim = pengirim laporan (server tidak menghitung damage sendiri —
 *      damage diputuskan gamemode/client, seperti SA-MP).
 *   2. Server memvalidasi lalu broadcast 0x47 DEATH_MSG ke SEMUA pemain
 *      (kill feed ala SA-MP bersifat global, tidak di-scope vw/interior)
 *      + menyalakan event Lua onPlayerDeath(victimid, killerid, reason).
 *   3. Gamemode bisa mem-broadcast kill feed manual via
 *      romp.sendDeathMessage(killerid, victimid, weaponid) — ala SA-MP
 *      fungsi ini TIDAK menyalakan OnPlayerDeath.
 *
 * killerid = 255 (DEATH_KILLER_NONE) artinya tak ada pembunuh (jatuh/
 * crash/drowned/ledakan sendiri) — ala SA-MP INVALID_PLAYER_ID.
 * reason = id senjata ala SA-MP (1..46) + kode khusus 49..54
 * (49=crash tabrakan, 50=heliblades, 51=explosion, 53=drown, 54=splat).
 *
 * NPC: tidak punya client untuk melapor, jadi kematian NPC hanya bisa
 * dimunculkan gamemode via romp.sendDeathMessage (manual).
 *
 * Protokol (lihat PROTOCOL.md):
 *   0x46 DEATH_REPORT (client -> server):
 *     u8 killerid | u8 reason            (3 byte frame, 2 byte payload)
 *     victim = pengirim. killerid 255 = tak ada pembunuh.
 *   0x47 DEATH_MSG (server -> semua client):
 *     u8 killerid | u8 victimid | u8 reason  (4 byte frame, 3 byte payload)
 */
#pragma once

#include "romp_server.h"

/* killerid 255 = tak ada pembunuh (ala SA-MP INVALID_PLAYER_ID). */
#define DEATH_KILLER_NONE 255

/* Reason maksimal yang diterima (id senjata 1..46 + kode khusus 49..54). */
#define DEATH_REASON_MAX 60

/* Tangani paket 0x46 DEATH_REPORT dari client c.
 * Validasi killerid (255 atau pemain yang connect) & reason (0..60);
 * bila lolos -> broadcast 0x47 ke semua pemain + event Lua
 * onPlayerDeath(victimid, killerid, reason). Laporan tak valid dibuang
 * diam-diam. */
void death_on_report(Client *c, const uint8_t *p, size_t n);

/* Broadcast manual kill feed ala SA-MP SendDeathMessage (dipakai Lua
 * romp.sendDeathMessage): tidak menyalakan event onPlayerDeath.
 * killerid 255 = tak ada pembunuh; victimid harus pemain yang connect;
 * reason 0..60. Kembalikan 1 bila berhasil dibroadcast; 0 bila argumen
 * tak valid. */
int death_send(int killerid, int victimid, int reason);
