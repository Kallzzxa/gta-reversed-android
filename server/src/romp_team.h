/*
 * RO-MP team pemain ala SA-MP (SetPlayerTeam / GetPlayerTeam) — server.
 *
 * Alur ala SA-MP:
 *   1. Gamemode memanggil romp.setPlayerTeam(pid, team) — mis. saat
 *      pemain join gang A (tim 1) atau gang B (tim 2). Server memvalidasi
 *      team 0..255 (255 = TEAM_NONE, tak punya tim ala SA-MP NO_TEAM),
 *      menyimpan state otoritatif di Client.team, lalu broadcast 0x53
 *      TEAM_SET ke SEMUA pemain yang connect. Tim bersifat global ala
 *      SA-MP: semua orang melihat warna tim pemain lain (scoreboard,
 *      marker, nametag), tidak di-scope virtual world/interior.
 *   2. Perubahan menyalakan event Lua onPlayerTeamChange(pid, oldteam,
 *      newteam) — perpanjangan RO-MP (SA-MP tak punya event ini) agar
 *      gamemode bisa menjalankan logika gangwar/dm: pindah warna marker,
 *      respawn di base tim, dsb.
 *   3. Pemain yang baru join di-resync tim semua pemain yang connect
 *      (team_sync_join), termasuk timnya sendiri (default TEAM_NONE).
 *
 * NPC didukung: state + broadcast jalan (paket ke NPC sendiri no-op
 * karena tak punya socket).
 *
 * Protokol (lihat server/PROTOCOL.md):
 *   0x53 TEAM_SET (server -> semua client):
 *     u8 pid | u8 team              (3 byte frame, 2 byte payload)
 *     team 0..254 = id tim, 255 = TEAM_NONE (tak punya tim).
 */
#pragma once

#include "romp_server.h"

/* 255 = tak punya tim ala SA-MP NO_TEAM (a_samp.inc). Default pemain baru. */
#define TEAM_NONE 255

/* 1 = team 0..255 valid, 0 = tidak. */
int team_valid_team(int team);

/* Set tim pemain ala SA-MP SetPlayerTeam.
 * Simpan state otoritatif, broadcast 0x53 ke semua pemain, nyalakan event
 * Lua onPlayerTeamChange(pid, old, new).
 * Kembalikan 1 bila berhasil; 0 bila pemain tak valid / team di luar
 * 0..255 (tanpa broadcast, tanpa event, state tak berubah). */
int team_set(Client *c, int team);

/* Get tim ala SA-MP GetPlayerTeam; -1 bila pemain tak valid. */
int team_get(const Client *c);

/* Inisialisasi tim (TEAM_NONE) untuk pemain baru / NPC baru. */
void team_init(Client *c);

/* Bersihkan state saat pemain leave (kembalikan TEAM_NONE agar tak bocor
 * bila struct dipakai ulang). Tidak mengirim paket apa pun. */
void team_on_player_leave(Client *c);

/* Resync tim semua pemain yang connect ke pemain yang baru join
 * (termasuk miliknya sendiri). */
void team_sync_join(Client *c);
