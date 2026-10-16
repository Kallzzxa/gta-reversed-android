/*
 * RO-MP ApplyAnimation ala SA-MP — header.
 *
 * SA-MP ApplyAnimation(playerid, animlib[], animname[], loop, lockx, locky,
 * freeze, time, forcesync) memutar animasi GTA pada ped pemain yang terlihat
 * pemain lain. RO-MP memakai pendekatan server-ototritatif yang sama dengan
 * fitur lain: state animasi disimpan di struct Client, dan server broadcast
 * paket 0x4E ANIM_SET / 0x4F ANIM_CLEAR ke viewer yang melihat pemain itu
 * (vw+interior scoped ala relay posisi 0x11) + menyalakan event Lua.
 *
 * 0x4E ANIM_SET   (Server -> Client) [u8 pid][u8 liblen][lib][u8 namelen][name]
 *                                       [u8 flags][u16 timems LE]
 *                                       flags bit0=loop bit1=lockx
 *                                       bit2=locky bit3=freeze bit4=forcesync
 *                                       liblen/namelen 1..32
 *                                       7..71 byte frame
 * 0x4F ANIM_CLEAR (Server -> Client) [u8 pid]    2 byte frame
 *                                       (ala SA-MP ClearAnimations)
 *
 * Animasi bersifat momenter ala SA-MP: ClearAnimations di server hanya
 * menghentikan di client; saat pemain baru join ia menerima resync 0x4E
 * untuk semua pemain yang sedang beranimasi & terlihat (agar animasi tak
 * "macet"), dan saat pemain pindah interior/vw state di-resync dua arah.
 * Saat pemain leave state di-reset diam-diam (P_LEAVE sudah menghapus ped
 * di semua client).
 * NPC boleh diberi animasi (state + broadcast jalan, tanpa socket).
 */
#ifndef ROMP_ANIM_H
#define ROMP_ANIM_H

#include <stdint.h>
#include <stddef.h>

/* Panjang maksimum nama library & nama animasi (ala SA-MP: string <= 32). */
#define ANIM_MAX_LEN 32

/* Flag animasi ala SA-MP ApplyAnimation (bitmask u8). */
#define ANIM_FLAG_LOOP      1
#define ANIM_FLAG_LOCKX     2
#define ANIM_FLAG_LOCKY     4
#define ANIM_FLAG_FREEZE    8
#define ANIM_FLAG_FORCESYNC 16

struct Client;

/* Reset state animasi pemain (dipanggil dari path join & NPC). */
void anim_init(struct Client *c);

/* Terapkan animasi ke pemain: simpan state otoritatif + broadcast 0x4E
 * vw-scoped + event Lua onPlayerApplyAnimation. Idempoten bila state
 * persis sama (tanpa broadcast ulang). return 1 sukses, 0 bila argumen
 * tak valid / pemain tak valid. */
int anim_apply(struct Client *c, const char *lib, const char *name,
               uint8_t flags, uint16_t timems);

/* Hentikan animasi pemain: broadcast 0x4F vw-scoped + event Lua
 * onPlayerClearAnimations. return 1 sukses, 0 bila pemain tak sedang
 * beranimasi / tak valid. */
int anim_clear(struct Client *c);

/* Reset diam-diam saat pemain leave (tanpa broadcast/event — ped sudah
 * dihapus P_LEAVE). */
void anim_leave(struct Client *c);

/* Resync ke pemain yang baru join: kirim 0x4E untuk semua pemain yang
 * sedang beranimasi & terlihat oleh dst. */
void anim_sync_join(struct Client *dst);

/* Pindah interior/vw: kirim 0x4F ke viewer lama & 0x4E ke viewer baru
 * (state idempoten — selalu di-broadcast ulang bila sedang beranimasi). */
void anim_on_world_change(struct Client *c);

/* 1 bila pemain sedang punya animasi aktif. */
int anim_active(const struct Client *c);

#endif /* ROMP_ANIM_H */
