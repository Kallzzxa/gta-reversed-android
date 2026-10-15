/*
 * RO-MP uang pemain otoritatif ala SA-MP (SetPlayerMoney / GivePlayerMoney /
 * ResetPlayerMoney / GetPlayerMoney) — server.
 *
 * Alur ala SA-MP:
 *   1. Gamemode memanggil romp.setPlayerMoney(pid, amount) /
 *      romp.givePlayerMoney(pid, delta) / romp.resetPlayerMoney(pid).
 *      Server menyimpan state otoritatif di Client.money, lalu mengirim
 *      0x49 MONEY_SET HANYA ke pemain bersangkutan (per-pemain — uang hanya
 *      tampil di HUD milik sendiri ala SA-MP, orang lain tak melihatnya).
 *   2. Perubahan menyalakan event Lua onPlayerMoneyChange(pid, oldamount,
 *      newamount) — perpanjangan RO-MP (SA-MP tak punya event ini) agar
 *      gamemode bisa mencatat transaksi (log bank, payday, dsb).
 *   3. Pemain yang baru join di-resync saldo miliknya sendiri
 *      (money_sync_join).
 *
 * Anti-dupe dasar: server satu-satunya sumber kebenaran — client tidak
 * pernah bisa mengubah uangnya sendiri (tak ada paket client -> server
 * untuk uang). Semua perubahan lewat API server; rentang di-clamp ke
 * MONEY_MIN..MONEY_MAX sehingga overflow penjumlahan tak mungkin terjadi
 * (money_give menjenuhkan/saturate, bukan melimpah).
 *
 * Protokol (lihat server/PROTOCOL.md):
 *   0x49 MONEY_SET (server -> client, hanya ke pemilik):
 *     u8 pid | s32 amount LE       (6 byte frame, 5 byte payload)
 *     amount MONEY_MIN..MONEY_MAX.
 */
#pragma once

#include "romp_server.h"

/* Rentang saldo ala HUD GTA:SA (9 digit, muat di tampilan). */
#define MONEY_MIN (-999999999)
#define MONEY_MAX  999999999

/* 1 = amount dalam rentang MONEY_MIN..MONEY_MAX, 0 = tidak. */
int money_valid_amount(int64_t amount);

/* Set saldo pemain ala SA-MP SetPlayerMoney.
 * Simpan state otoritatif, kirim 0x49 ke pemain tsb, nyalakan event Lua
 * onPlayerMoneyChange(pid, old, new). Kembalikan 1 bila berhasil;
 * 0 bila pemain tak valid / amount di luar rentang (tanpa paket, tanpa
 * event, state tak berubah). */
int money_set(Client *c, int32_t amount);

/* Tambah/kurangi saldo ala SA-MP GivePlayerMoney. Hasil dijenuhkan ke
 * MONEY_MIN..MONEY_MAX (anti overflow). Event tetap menyala dengan
 * old/new hasil saturasi. 1 = sukses, 0 = pemain tak valid. */
int money_give(Client *c, int64_t delta);

/* Reset saldo ke 0 ala SA-MP ResetPlayerMoney. 1 = sukses. */
int money_reset(Client *c);

/* Ambil saldo ala SA-MP GetPlayerMoney ke *out. 1 = sukses,
 * 0 = pemain tak valid (out tak disentuh). */
int money_get(const Client *c, int32_t *out);

/* Inisialisasi saldo (0) untuk pemain baru / NPC baru. */
void money_init(Client *c);

/* Bersihkan state saat pemain leave (kembalikan 0 agar tak bocor bila
 * struct dipakai ulang). Tidak mengirim paket apa pun. */
void money_on_player_leave(Client *c);

/* Resync saldo miliknya sendiri ke pemain yang baru join. */
void money_sync_join(Client *c);
