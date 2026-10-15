/*
 * RO-MP player marker ala SA-MP (ShowPlayerMarkers /
 * SetPlayerMarkerForPlayer / GetPlayerColor) — server.
 *
 * Setiap pemain punya warna marker radar sendiri (0xRRGGBBAA), otomatis
 * dari palet berdasar id saat connect (ala SA-MP yang memberi warna
 * otomatis — bisa dibaca gamemode via romp.getPlayerColor). Gamemode bisa
 * mengganti warna sendiri (romp.setPlayerColor) atau menimpa warna pemain
 * lain menurut pandangan satu viewer (romp.setPlayerMarkerForPlayer —
 * override disimpan per-viewer, bukan global).
 *
 * Mode marker global (ala SA-MP ShowPlayerMarkers):
 *   0 = MARKER_MODE_OFF      (tak ada marker yang dikirim)
 *   1 = MARKER_MODE_GLOBAL   (semua pemain saling melihat marker)
 *   2 = MARKER_MODE_STREAMED (default; sama dengan global di RO-MP karena
 *                             belum ada streaming berbasis jarak)
 *
 * Protokol (server -> client, lihat PROTOCOL.md):
 *   0x44 MARKER_SET:
 *     u8 pid | u32 color (LE)        (6 byte frame, 5 byte payload)
 *     Warna marker EFEKTIF pemain pid menurut pandangan client penerima
 *     (override per-viewer sudah diterapkan). Dikirim vw-scoped: hanya ke
 *     client yang se-vw & se-interior dengan target.
 *   0x45 MARKER_MODE:
 *     u8 mode                        (2 byte frame, 1 byte payload)
 *     Mode marker global berubah. Broadcast ke semua client.
 *
 * NPC: state warna + override jalan normal (muncul di daftar pemain);
 * paket ke NPC jadi no-op via client_send_frame.
 */
#pragma once

#include "romp_server.h"

/* Mode marker ala SA-MP (PLAYER_MARKERS_MODE_* di a_samp.inc). */
#define MARKER_MODE_OFF      0
#define MARKER_MODE_GLOBAL   1
#define MARKER_MODE_STREAMED 2

/* 1 = mode valid (0..2), 0 = tidak. */
int marker_valid_mode(int mode);

/* Mode marker global saat ini. */
int marker_get_mode(void);

/* Set mode marker global (ala SA-MP ShowPlayerMarkers). Broadcast 0x45 ke
 * semua client; bila mode dinyalakan kembali, semua warna efektif
 * dikirim ulang. Kembalikan 1 bila berhasil; 0 bila mode tak valid. */
int marker_set_mode(int mode);

/* Inisialisasi state marker pemain baru / NPC: warna default dari palet
 * otomatis + override dikosongkan. Dipanggil saat handshake & npc_connect. */
void marker_init(Client *c);

/* Bersihkan state marker saat pemain keluar: hapus override pemain lain
 * yang menarget id ini (agar id yang dipakai ulang tak mewarisi override
 * basi) + bebaskan array override miliknya. Dipanggil dari do_leave &
 * npc_disconnect. */
void marker_on_player_leave(Client *c);

/* Set warna marker milik pemain (ala SA-MP — tak ada fungsi bawaan untuk
 * ini, tapi GetPlayerColor butuh state yang bisa diubah; dipakai juga
 * untuk demo). Broadcast 0x44 vw-scoped dengan warna efektif per-viewer.
 * Kembalikan 1 bila berhasil; 0 bila pemain tak valid. */
int marker_set_color(Client *c, uint32_t color);

/* Ambil warna marker milik pemain (ala SA-MP GetPlayerColor).
 * Kembalikan warna 0xRRGGBBAA, atau 0 bila pemain tak valid. */
uint32_t marker_get_color(const Client *c);

/* Warna marker efektif target menurut pandangan viewer (override
 * per-viewer bila ada, else warna milik target). */
uint32_t marker_effective(const Client *viewer, const Client *target);

/* Set override warna marker: viewer melihat showpid dengan warna `color`
 * (ala SA-MP SetPlayerMarkerForPlayer). Mengirim 0x44 hanya ke viewer bila
 * ia sedang melihat target & mode aktif. Kembalikan 1 bila berhasil;
 * 0 bila viewer/target tak valid. */
int marker_set_for(Client *viewer, int showpid, uint32_t color);

/* Hapus override (kembalikan ke warna milik target). Kembalikan 1 bila
 * ada override yang dihapus; 0 bila tak ada / argumen tak valid. */
int marker_clear_for(Client *viewer, int showpid);

/* Hapus SEMUA override milik viewer. Kembalikan jumlah yang dihapus
 * (dipakai resync sederhana). */
int marker_clear_all_for(Client *viewer);

/* Resync ke pemain yang baru join: kirim 0x45 (mode) + 0x44 untuk semua
 * pemain yang terlihat olehnya (warna efektif per-viewer). */
void marker_sync_join(Client *c);

/* Broadcast 0x44 warna target ke semua viewer yang melihatnya
 * (mode != OFF). Dipanggil setelah P_JOIN di do_join & npc_connect agar
 * viewer lama tahu warna pemain yang baru bergabung. */
void marker_broadcast_color(const Client *target, const Client *except);

/* Resync dua arah saat pemain pindah interior/vw: kirim ulang 0x44 untuk
 * pasangan yang sekarang saling terlihat (idempoten — aman dipanggil
 * walau visibilitas tak berubah). Dipanggil dari player_set_world. */
void marker_sync_world(Client *c);
