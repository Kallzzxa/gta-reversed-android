/*
 * RO-MP spectate ala SA-MP (TogglePlayerSpectating / PlayerSpectatePlayer /
 * PlayerSpectateVehicle) — server.
 *
 * Alur ala SA-MP:
 *   1. Gamemode memanggil romp.togglePlayerSpectating(pid, on) /
 *      romp.playerSpectatePlayer(pid, targetpid[, mode]) /
 *      romp.playerSpectateVehicle(pid, vid[, mode]). Server menyimpan state
 *      otoritatif di struct Client (spectating/type/target/mode), lalu
 *      broadcast 0x4B SPECTATE vw-scoped ke vw si penonton.
 *   2. Efek di client:
 *      - client si penonton (pid == id sendiri): masuk mode kamera mengikuti
 *        target (mode 0=normal, 1=fixed, 2=side ala SA-MP SPECTATE_MODE_*).
 *        Target type 0 (none) = kamera spectate tanpa target (bebas).
 *      - client lain yang se-vw: sembunyikan ped si penonton
 *        (spectating=1) / tampilkan lagi (spectating=0). Pemain tetap ada
 *        di daftar pemain & scoreboard ala SA-MP.
 *   3. Selama spectate, paket posisi 0x10 dari si penonton DIBUANG server
 *      (posisi beku ala SA-MP — ped tak ada di dunia, tak perlu sync;
 *      juga menghindari false-positive anti-cheat).
 *   4. Perubahan menyalakan event Lua onPlayerSpectate(pid, spectating,
 *      targettype, target, mode) — perpanjangan RO-MP (SA-MP tak punya
 *      event ini) agar gamemode bisa membatasi (mis. larang spectate saat
 *      event) atau mencatat.
 *   5. Resync: pemain yang baru join menerima 0x4B untuk semua yang sedang
 *      spectate di vw-nya (spectate_sync_join); saat penonton pindah vw,
 *      state di-broadcast ulang di vw baru.
 *
 * Aturan validasi:
 *   - NPC tak bisa jadi penonton (tak punya kamera) -> false.
 *     NPC BOLEH jadi target (muncul di daftar pemain seperti biasa).
 *   - Target pemain: harus connect & handshake, bukan diri sendiri,
 *     dan se-vw dengan penonton (stream posisi target hanya jalan se-vw).
 *   - Target kendaraan: harus valid & tidak hancur, se-vw dengan penonton.
 *   - Mode 0..2; di luar itu -> false.
 *   - playerSpectatePlayer/Vehicle otomatis mengaktifkan mode spectate bila
 *     belum aktif (ramah ala RO-MP; SA-MP mewajibkan TogglePlayerSpectating
 *     dulu — di sini tak wajib).
 *   - Idempoten: set ke nilai yang sama -> true tanpa broadcast/event.
 *
 * Auto-stop (broadcast 0x4B spectating=0 + event):
 *   - target pemain disconnect (do_leave / npc_disconnect),
 *   - target kendaraan di-destroy,
 *   - vw berubah sehingga penonton & target tak lagi se-vw (salah satu
 *     pihak pindah — dicek di spectate_on_world_change /
 *     spectate_on_vehicle_world_change),
 *   - penonton disconnect (cleanup diam-diam, P_LEAVE sudah menghapus ped).
 *
 * Protokol (lihat server/PROTOCOL.md):
 *   0x4B SPECTATE (server -> client, broadcast vw-scoped ke vw penonton):
 *     u8 pid | u8 spectating | u8 target_type | u16 target LE | u8 mode
 *     (7 byte frame, 6 byte payload)
 *     target_type: 0=none, 1=pemain (target=pid), 2=kendaraan (target=vid).
 *     mode: 0=normal, 1=fixed, 2=side (ala SA-MP SPECTATE_MODE_*).
 */
#pragma once

#include "romp_server.h"

/* Tipe target spectate (u8 target_type di 0x4B). */
#define SPECTATE_TARGET_NONE    0
#define SPECTATE_TARGET_PLAYER  1
#define SPECTATE_TARGET_VEHICLE 2

/* Mode kamera spectate ala SA-MP (SPECTATE_MODE_* di a_samp.inc). */
#define SPECTATE_MODE_NORMAL 0
#define SPECTATE_MODE_FIXED  1
#define SPECTATE_MODE_SIDE   2

/* 1 = mode valid (0..2), 0 = tidak. */
int spectate_valid_mode(int mode);

/* Inisialisasi state spectate (mati) untuk pemain baru / NPC baru. */
void spectate_init(Client *c);

/* 1 = pemain sedang spectate, 0 = tidak / pemain tak valid. */
int spectate_is(const Client *c);

/* Ambil state spectate ke *type, *target, *mode (boleh NULL).
 * 1 = pemain valid & sedang spectate, 0 = tidak. */
int spectate_get(const Client *c, int *type, int *target, int *mode);

/* ala SA-MP TogglePlayerSpectating. on=1 -> masuk mode spectate (target
 * tetap/none); on=0 -> keluar & hapus target. Broadcast 0x4B + event bila
 * berubah. 1 = sukses, 0 = pemain tak valid / NPC (tak bisa spectate). */
int spectate_toggle(Client *c, int on);

/* ala SA-MP PlayerSpectatePlayer. Otomatis masuk mode spectate bila belum.
 * 1 = sukses (broadcast 0x4B + event bila berubah), 0 = validasi gagal
 * (target tak valid / diri sendiri / beda vw / mode invalid / NPC). */
int spectate_player(Client *c, int target_pid, int mode);

/* ala SA-MP PlayerSpectateVehicle. Otomatis masuk mode spectate bila belum.
 * 1 = sukses, 0 = kendaraan tak valid/hancur / beda vw / mode invalid. */
int spectate_vehicle(Client *c, int vid, int mode);

/* Hentikan spectate secara paksa (dipakai auto-stop): broadcast 0x4B
 * spectating=0 + event. 1 = memang sedang spectate & dihentikan. */
int spectate_stop(Client *c);

/* Broadcast frame 0x4B state terkini c ke vw-nya (termasuk c sendiri). */
void spectate_broadcast(Client *c);

/* Kirim satu frame 0x4B tentang `spec` ke `dst` (dipakai resync join). */
void spectate_send_to(Client *dst, const Client *spec);

/* Resync ke pemain yang baru join: 0x4B untuk semua yang sedang spectate
 * dan terlihat oleh joiner. */
void spectate_sync_join(Client *c);

/* Cleanup saat penonton disconnect: hentikan diam-diam (tanpa broadcast —
 * P_LEAVE sudah menghapus ped) + hentikan semua penonton yang menarget c. */
void spectate_on_player_leave(Client *c);

/* Hentikan semua penonton yang menarget kendaraan vid (di-destroy). */
void spectate_on_vehicle_destroy(int vid);

/* Re-validasi semua relasi spectate setelah vw/interior pemain berubah:
 * relasi yang kini cross-vw (atau targetnya hilang) di-stop otomatis. */
void spectate_on_world_change(void);

/* Re-validasi relasi yang menarget kendaraan vid setelah vw-nya berubah. */
void spectate_on_vehicle_world_change(int vid);
