/*
 * RO-MP OnPlayerKeyStateChange ala SA-MP — header.
 *
 * Client melaporkan state tombol yang ditekan (bitmask u16 ala SA-MP KEY_*)
 * lewat paket 0x4C KEY_STATE setiap kali berubah. Server menyimpan state
 * otoritatif per-pemain (c->keys), lalu broadcast 0x4D KEY_SYNC
 * (pid + u16 keys) ke semua viewer yang melihat pemain itu (vw+interior
 * scoped ala relay posisi 0x11) + menyalakan event Lua
 * onPlayerKeyStateChange(pid, newkeys, oldkeys).
 *
 * Rate limit: laporan lebih cepat dari 50 ms dibuang diam-diam (maks 20/dtk).
 * Frame tak valid (payload != 2 byte) dibuang diam-diam.
 *
 * 0x4C KEY_STATE  (Client -> Server)  [u16 keys LE]                    3 byte frame
 * 0x4D KEY_SYNC   (Server -> Client)  [u8 pid][u16 keys LE]             4 byte frame
 *
 * State keys bersifat momenter ala SA-MP: tidak di-resync saat join baru
 * (joiner tahu setelah ada perubahan) dan di-reset 0 saat leave.
 * NPC tak punya socket sehingga tak bisa lapor — romp.getPlayerKeys untuk
 * NPC selalu 0 (default).
 */
#ifndef ROMP_KEY_H
#define ROMP_KEY_H

#include <stdint.h>
#include <stddef.h>

/* Konstanta tombol ala SA-MP (bitmask, mirrors PAWN KEY_*). */
#define KEY_ACTION            1
#define KEY_CROUCH            2
#define KEY_FIRE              4
#define KEY_SPRINT            8
#define KEY_SECONDARY_ATTACK  16
#define KEY_JUMP              32
#define KEY_LOOK_RIGHT        64
#define KEY_HANDBRAKE         128
#define KEY_LOOK_LEFT         256
#define KEY_SUBMISSION        512
#define KEY_WALK              1024
#define KEY_ANALOG_UP         2048
#define KEY_ANALOG_DOWN       4096
#define KEY_ANALOG_RIGHT      8192
#define KEY_ANALOG_LEFT       16384
#define KEY_YES               65536
#define KEY_NO                131072
#define KEY_CTRL_BACK         262144

/* Rate limit minimal antar laporan key state (ms). */
#define KEY_MIN_MS 50

struct Client;

/* Inisialisasi state key pemain baru (dipanggil dari path join & NPC). */
void key_init(struct Client *c);

/* Tangani paket 0x4C KEY_STATE dari client. */
void key_on_state(struct Client *c, const uint8_t *p, size_t n);

/* Ambil state key terakhir pemain (0 bila tak valid). */
uint16_t key_get(const struct Client *c);

#endif /* ROMP_KEY_H */
