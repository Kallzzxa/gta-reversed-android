/*
 * RO-MP actor statis ala SA-MP (CreateActor/DestroyActor/ApplyActorAnimation).
 *
 * Actor adalah ped statis server-side: lebih ringan dari NPC (ConnectNPC)
 * karena tidak menempati slot pemain, tidak punya socket, dan tidak
 * di-sync posisinya secara periodik — posisi hanya berubah lewat Lua.
 *
 * Server bersifat otoritatif untuk health actor: tembakan yang dilaporkan
 * client via 0x17 WPN_SHOT (hittype 4 = actor) divalidasi lalu damage
 * dihitung server berdasarkan weaponid (tabel damage ala GTA:SA).
 * Actor yang invulnerable (SetActorInvulnerable) tidak berkurang health-nya,
 * tapi event onPlayerGiveDamageActor tetap dipicu (ala SA-MP).
 *
 * Protokol (server -> client, lihat server/PROTOCOL.md):
 *   0x32 ACTOR_SPAWN:  u16 actorid | u16 modelid (skin 0-311) |
 *                      f32 x,y,z | f32 angle | u16 health | u8 invuln |
 *                      u32 vw
 *   0x33 ACTOR_DESPAWN: u16 actorid
 *   0x34 ACTOR_POS:    u16 actorid | f32 x,y,z | f32 angle
 *   0x35 ACTOR_ANIM:   u16 actorid | u8 animlen | char anim[animlen]
 *                      (animlen = 0 berarti animasi dibersihkan)
 *   0x36 ACTOR_HEALTH: u16 actorid | u16 health
 */
#pragma once

#include "romp_server.h"

#define ACTOR_MAX       1000  /* maks actor aktif (ala SA-MP MAX_ACTORS) */
#define ACTOR_ANIM_MAX  64    /* maks byte nama animasi (u8 animlen) */
#define ACTOR_MODEL_MAX 311   /* skin id SA-MP 0-311 */
#define ACTOR_HEALTH_MAX 100  /* health actor 0-100 ala SA-MP */

typedef struct Actor {
    int      id;                  /* actorid, mulai dari 1 */
    int      modelid;             /* skin model 0-311 */
    float    x, y, z;            /* posisi dunia */
    float    angle;               /* facing angle */
    int      health;              /* 0-100; 0 = mati */
    int      invulnerable;        /* 1 = kebal (damage diabaikan) */
    uint32_t vw;                 /* virtual world */
    char     anim[ACTOR_ANIM_MAX + 1]; /* animasi aktif; "" = tidak ada */
    struct Actor *next;
} Actor;

/* ---- Registry ---- */
Actor *actor_by_id(int id);
int    actor_is_valid(int id);
Actor *actor_first(void);      /* iterasi: NULL = tidak ada */
int    actor_count(void);
/* Buat actor (broadcast 0x32 ke viewer vw).
 * Kembalikan actorid (>0) atau -1 bila gagal (id habis / model invalid). */
int actor_create(int modelid, float x, float y, float z, float angle);
/* Hancurkan actor (broadcast 0x33 ke viewer vw). 1 = sukses. */
int actor_destroy(int id);
/* Set posisi + angle instan (broadcast 0x34 ke viewer vw). 1 = sukses. */
int actor_set_pos(int id, float x, float y, float z, float angle);
/* Set health 0-100 (broadcast 0x36 ke viewer vw bila berubah).
 * TIDAK memicu onActorDeath — itu hanya dari damage. 1 = sukses. */
int actor_set_health(int id, int health);
/* Set invulnerable (broadcast ulang 0x32 agar client tahu flag baru).
 * 1 = sukses. */
int actor_set_invulnerable(int id, int invuln);
/* Terapkan animasi (broadcast 0x35). anim boleh NULL/kosong = clear.
 * 1 = sukses. */
int actor_apply_anim(int id, const char *anim);
/* Hapus animasi (broadcast 0x35 animlen=0). 1 = sukses. */
int actor_clear_anim(int id);
/* Set virtual world (despawn vw lama + spawn vw baru). 1 = sukses. */
int actor_set_vw(int id, uint32_t vw);
/* Damage actor oleh pemain (dipanggil dari validasi WPN_SHOT hittype 4).
 * Damage dihitung dari weaponid (tabel ala GTA:SA); invulnerable diabaikan.
 * Memicu event Lua onPlayerGiveDamageActor(pid, actorid, amount, weaponid,
 * bodypart=0); bila health mencapai 0, broadcast 0x36 + event
 * onActorDeath(actorid, killerid). 1 = damage diterapkan, 0 = id invalid. */
int actor_damage(int pid, int actorid, int weaponid);

/* ---- Sync jaringan ---- */
/* Kirim 0x32 semua actor yang se-vw ke client baru (dipakai saat join). */
void actor_sync_join(Client *c);
/* Resync actor saat pemain pindah vw: despawn (ke c) actor vw lama +
 * spawn (ke c) actor vw baru. Dipakai player_set_world. */
void actor_sync_world(Client *c, uint32_t old_vw);
