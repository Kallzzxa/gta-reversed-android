/*
 * RO-MP sinkronisasi senjata & tembakan ala SA-MP (combat dasar).
 *
 * Server bersifat otoritatif untuk INVENTORY senjata pemain (romp.giveWeapon
 * / resetPlayerWeapons): client hanya boleh menembak dengan senjata yang
 * diberikan server. Tembakan di-relay mentah ke client lain (paket 0x17 ->
 * 0x18), mirip relay posisi 0x10 -> 0x11; damage tidak dihitung server
 * (gamemode yang memutuskan lewat event onPlayerWeaponShot).
 *
 * Slot senjata ala SA-MP (GetPlayerWeaponData):
 *   0: 0,1   1: 2-9   2: 22-24   3: 25-27   4: 28,29,32   5: 30,31
 *   6: 33,34  7: 35-38  8: 16-18,39  9: 14  10: 42-44  11: 45  12: 40
 *
 * hittype: 0 = tidak kena, 1 = pemain, 2 = kendaraan, 3 = objek/dunia,
 *           4 = actor (damage dihitung server, lihat romp_actor.h). */
#pragma once

#include "romp_server.h"

#define WPN_SLOTS      13
#define WPN_MAX_ID     46   /* id senjata SA-MP 1..46 (0 = kosong/tangan) */
#define WPN_SHOT_MIN_MS 40  /* anti-spam: min jarak antar tembakan */

/* Slot SA-MP untuk weaponid. -1 = invalid (0 atau >46 atau tak dikenal). */
int weapon_slot(int weaponid);

/* Beri senjata ke pemain (kirim 0x19 WPN_SET ke client). 1 = sukses. */
int weapon_give(Client *c, int weaponid, int ammo);
/* Hapus senjata di slot (kirim 0x19 dengan weaponid 0). 1 = sukses. */
int weapon_remove_slot(Client *c, int slot);
/* Hapus semua senjata pemain (kirim 0x19 x 13). */
void weapon_reset(Client *c);

/* Proses paket 0x17 WPN_SHOT dari client. Validasi -> relay 0x18 + event Lua
 * onPlayerWeaponShot(pid, weaponid, hittype, hitid, x, y, z). */
void weapon_on_packet(Client *c, const uint8_t *p, size_t n);
