/*
 * RO-MP weapon skill ala SA-MP (SetPlayerSkillLevel/GetPlayerSkillLevel)
 * — server.
 *
 * Level skill senjata per pemain (0..10 ala SA-MP), tersimpan otoritatif
 * di state server (Client.skill[]). Level 0..1000 ala SA-MP; pemain baru
 * (termasuk NPC) mulai dari SKILL_DEFAULT_LEVEL (1).
 *
 * Skill bersifat per-pemain dan TIDAK di-broadcast: hanya pemiliknya yang
 * perlu tahu (skill mempengaruhi handling senjata client-nya sendiri,
 * seperti akurasi & kecepatan bidik). Paket 0x43 SKILL_SET dikirim hanya
 * ke pemain target via client_send_frame() (no-op untuk NPC — NPC tak
 * punya socket, state-nya tetap tersimpan dan bisa dibaca gamemode).
 *
 * Tak ada resync saat join yang diperlukan: pemain baru selalu mulai dari
 * default. Tak ada scope virtual world (per-pemain, seperti audio stream).
 * State di-reset ke default saat pemain keluar.
 *
 * Protokol (server -> client, lihat PROTOCOL.md):
 *   0x43 SKILL_SET:
 *     u8 pid | u8 skill | u16 level (LE)      (5 byte frame, 4 byte payload)
 */
#pragma once

#include "romp_server.h"

/* SKILL_COUNT didefinisikan di romp_server.h (dipakai ukuran field
 * Client.skill). */

/* Skill senjata ala SA-MP (WEAPONSKILL_* di a_samp.inc). */
#define SKILL_PISTOL            0
#define SKILL_PISTOL_SILENCED   1
#define SKILL_DESERT_EAGLE      2
#define SKILL_SHOTGUN           3
#define SKILL_SAWNOFF_SHOTGUN   4
#define SKILL_SPAS12_SHOTGUN    5
#define SKILL_MICRO_UZI         6
#define SKILL_MP5               7
#define SKILL_AK47              8
#define SKILL_M4                9
#define SKILL_SNIPERRIFLE       10

/* Batas level ala SA-MP. */
#define SKILL_LEVEL_MIN 0
#define SKILL_LEVEL_MAX 1000
/* Level awal pemain baru (termasuk NPC). */
#define SKILL_DEFAULT_LEVEL 1

/* 1 = id skill valid (0..SKILL_COUNT-1), 0 = tidak. */
int skill_valid_id(int skill);

/* 1 = level valid (SKILL_LEVEL_MIN..SKILL_LEVEL_MAX), 0 = tidak. */
int skill_valid_level(int level);

/* Set level skill pemain (ala SA-MP SetPlayerSkillLevel).
 * Validasi: pemain valid & handshaked, skill 0..10, level 0..1000.
 * Menyimpan state otoritatif lalu mengirim 0x43 SKILL_SET ke pemain
 * tersebut saja. Kembalikan 1 bila berhasil; 0 bila argumen tak valid. */
int skill_set(Client *c, int skill, int level);

/* Ambil level skill pemain (ala SA-MP GetPlayerSkillLevel).
 * Kembalikan level 0..1000, atau -1 bila pemain/skill tak valid. */
int skill_get(const Client *c, int skill);

/* Inisialisasi semua skill ke SKILL_DEFAULT_LEVEL (dipanggil saat pemain
 * / NPC connect). */
void skill_init(Client *c);

/* Reset semua skill ke SKILL_DEFAULT_LEVEL saat pemain keluar (tanpa
 * kirim paket — skill bersifat per-pemain, tak ada yang perlu tahu). */
void skill_on_player_leave(Client *c);
