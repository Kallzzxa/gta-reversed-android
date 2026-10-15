/*
 * RO-MP attached object ala SA-MP (SetPlayerAttachedObject/
 * RemovePlayerAttachedObject) — server.
 *
 * Objek menempel di bone pemain (topi, kacamata, senjata di punggung,
 * dsb). Maks ATTACH_MAX_SLOTS (10) slot per pemain, index 0..9 ala SA-MP
 * (MAX_PLAYER_ATTACHED_OBJECTS). Set ulang slot yang sama menimpa
 * (ala SA-MP).
 *
 * Attach bersifat per-pemain dan vw-scoped: terlihat semua pemain yang
 * se-virtual-world dengan pemiliknya (objek ikut pemain). Broadcast via
 * broadcast_frame_vw() dengan vw pemilik, termasuk pemilik sendiri (dia
 * juga harus me-render attach-nya). Interior diabaikan seperti paket
 * kendaraan. State otoritatif server di Client.attach[10]; di-resync ke
 * pemain yang baru join / pindah vw, dibersihkan saat pemain keluar.
 *
 * NPC (ConnectNPC) bisa punya attach: state disimpan, broadcast ke pemain
 * lain tetap jalan (NPC tak punya socket — broadcast_frame_vw otomatis
 * melewati NPC).
 *
 * Protokol (server -> client, lihat PROTOCOL.md):
 *   0x3F ATTACH_OBJ set:
 *     u8 pid | u8 index | u32 modelid | u8 bone | f32 offx,offy,offz |
 *     f32 rotx,roty,rotz | f32 scalex,scaley,scalez   (44 byte frame)
 *   0x3F ATTACH_OBJ remove:
 *     u8 pid | u8 index | u32 modelid(=0)             (7 byte frame)
 * Client membedakan set vs remove dari panjang payload / modelid.
 */
#pragma once

#include "romp_server.h"

#define ATTACH_BONE_MIN 1   /* 1 = spine */
#define ATTACH_BONE_MAX 18  /* 18 = jaw */
/* Daftar bone ala SA-MP: 1=spine, 2=head, 3=l upper arm, 4=r upper arm,
 * 5=l hand, 6=r hand, 7=l thigh, 8=r thigh, 9=l foot, 10=r foot,
 * 11=r calf, 12=l calf, 13=l forearm, 14=r forearm, 15=l clavicle,
 * 16=r clavicle, 17=neck, 18=jaw. */

/* Tempelkan objek ke slot pemain (ala SA-MP SetPlayerAttachedObject).
 * index 0..ATTACH_MAX_SLOTS-1, modelid != 0, bone 1..18, tiap sumbu
 * scale > 0. Slot yang sama di-set ulang = ditimpa. Broadcast 0x3F (set)
 * vw-scoped. Kembalikan 1 bila berhasil; 0 bila argumen tidak valid
 * (pemain tak valid, index/bone/modelid/scale di luar batas). */
int attach_set(Client *c, int index, uint32_t modelid, uint8_t bone,
               const float off[3], const float rot[3], const float scale[3]);

/* Lepaskan objek dari slot pemain (ala SA-MP RemovePlayerAttachedObject).
 * Broadcast 0x3F (remove) vw-scoped. Kembalikan 1 bila berhasil; 0 bila
 * slot kosong atau index tidak valid. */
int attach_remove(Client *c, int index);

/* 1 = slot terisi, 0 = kosong / index tak valid. */
int attach_slot_used(Client *c, int index);

/* Resync ke pemain yang baru join: kirim frame set untuk semua slot
 * terisi milik pemain yang se-vw dengannya (termasuk milik NPC). */
void attach_sync_join(Client *c);

/* Resync saat pemain pindah vw: kirim frame remove untuk attach milik
 * pemain di vw lama, lalu frame set untuk attach milik pemain di vw baru. */
void attach_sync_world(Client *c, uint32_t old_vw);

/* Bersihkan state attach saat pemain keluar (tanpa kirim paket — client
 * lain tahu dari P_LEAVE; untuk NPC tak ada yang perlu dikirim). */
void attach_on_player_leave(Client *c);
