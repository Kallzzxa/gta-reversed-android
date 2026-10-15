/*
 * RO-MP sinkronisasi senjata & tembakan ala SA-MP — implementasi.
 * Lihat romp_weapon.h untuk desain protokol & API.
 */
#include "romp_weapon.h"
#include "romp_vehicle.h"
#include "romp_actor.h"
#include "romp_lua.h"
#include <string.h>
#include <math.h>

/* ---- Helper frame (duplikat kecil dari romp_server.c agar modul mandiri) ---- */
static void wput_u16(uint8_t *b, uint16_t v) { b[0] = v & 0xFF; b[1] = (v >> 8) & 0xFF; }
static void wput_f32(uint8_t *b, float v) { uint32_t u; memcpy(&u, &v, 4); wput_u16(b, (uint16_t)(u & 0xFFFF)); wput_u16(b + 2, (uint16_t)(u >> 16)); }
static uint16_t wget_u16(const uint8_t *b) { return (uint16_t)(b[0] | (b[1] << 8)); }
static float wget_f32(const uint8_t *b) {
    uint32_t u = (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
                 ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    float f; memcpy(&f, &u, 4); return f;
}

/* ---- Slot senjata ala SA-MP ---- */

int weapon_slot(int weaponid) {
    switch (weaponid) {
        case 0: case 1:   return 0;
        case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: return 1;
        case 22: case 23: case 24: return 2;
        case 25: case 26: case 27: return 3;
        case 28: case 29: case 32: return 4;
        case 30: case 31: return 5;
        case 33: case 34: return 6;
        case 35: case 36: case 37: case 38: return 7;
        case 16: case 17: case 18: case 39: return 8;
        case 14: return 9;
        case 42: case 43: case 44: return 10;
        case 45: return 11;
        case 40: return 12;
        default: return -1;
    }
}

/* ---------------- Inventory server-side ---------------- */

/* 0x19 WPN_SET: u8 slot | u16 weaponid | u16 ammo. Kirim ke pemain itu saja. */
static void send_wpn_set(Client *c, int slot, int weaponid, int ammo) {
    uint8_t b[6];
    b[0] = P_WPN_SET;
    b[1] = (uint8_t)slot;
    wput_u16(b + 2, (uint16_t)weaponid);
    wput_u16(b + 4, (uint16_t)ammo);
    client_send_frame(c, b, sizeof(b));
}

int weapon_give(Client *c, int weaponid, int ammo) {
    if (!c || !c->handshaked) return 0;
    int slot = weapon_slot(weaponid);
    if (slot < 0 || ammo < 0 || ammo > 9999) return 0;
    c->wpn[slot] = (uint16_t)weaponid;
    c->wpn_ammo[slot] = (uint16_t)ammo;
    c->wpn_slot = slot;
    send_wpn_set(c, slot, weaponid, ammo);
    return 1;
}

int weapon_remove_slot(Client *c, int slot) {
    if (!c || !c->handshaked || slot < 0 || slot >= WPN_SLOTS) return 0;
    c->wpn[slot] = 0;
    c->wpn_ammo[slot] = 0;
    if (c->wpn_slot == slot) c->wpn_slot = 0;
    send_wpn_set(c, slot, 0, 0);
    return 1;
}

void weapon_reset(Client *c) {
    if (!c || !c->handshaked) return;
    memset(c->wpn, 0, sizeof(c->wpn));
    memset(c->wpn_ammo, 0, sizeof(c->wpn_ammo));
    c->wpn_slot = 0;
    for (int s = 0; s < WPN_SLOTS; s++) send_wpn_set(c, s, 0, 0);
}

/* ---------------- Paket tembakan ---------------- */

/* Validasi jangkauan: titik tembak tidak boleh lebih dari MAX_RANGE dari
 * posisi shooter (anti teleport-bullet dasar). 0 = lolos. */
#define WPN_MAX_RANGE 400.0f

static int shot_in_range(Client *c, float x, float y, float z) {
    if (!c->pos_known) return 1; /* belum ada data posisi: tidak bisa cek */
    float dx = x - c->pos_x, dy = y - c->pos_y, dz = z - c->pos_z;
    return (dx*dx + dy*dy + dz*dz) <= WPN_MAX_RANGE * WPN_MAX_RANGE;
}

/* 0x17 WPN_SHOT (client -> server):
 *   u16 weaponid | u8 hittype | u16 hitid | f32 x,y,z (titik kena) */
void weapon_on_packet(Client *c, const uint8_t *p, size_t n) {
    if (!c->handshaked || n != 2 + 1 + 2 + 12) return; /* payload: u16,u8,u16,3x f32 */
    uint64_t now = now_ms();
    if (now - c->last_shot_ms < WPN_SHOT_MIN_MS) return; /* anti-spam */
    c->last_shot_ms = now;

    int weaponid = wget_u16(p);
    int hittype  = p[2];
    int hitid    = wget_u16(p + 3);
    float x = wget_f32(p + 5), y = wget_f32(p + 9), z = wget_f32(p + 13);

    int slot = weapon_slot(weaponid);
    if (slot < 0) return;                        /* senjata tidak dikenal */
    if (c->wpn[slot] != (uint16_t)weaponid) return; /* tidak dimiliki -> tolak */
    if (hittype > 4) return;
    if (!shot_in_range(c, x, y, z)) return;       /* di luar jangkauan */

    /* Validasi target bila kena sesuatu. */
    if (hittype == 1) { /* pemain */
        Client *t = client_by_id(hitid);
        if (!t || t == c) return;
    } else if (hittype == 2) { /* kendaraan */
        if (!vehicle_by_id(hitid)) return;
    } else if (hittype == 4) { /* actor */
        if (!actor_is_valid(hitid)) return;
    }
    c->wpn_slot = slot; /* senjata terakhir dipakai */

    /* 0x18 WPN_SYNC broadcast (tidak di-echo ke penembak):
     *   u16 shooterid | u16 weaponid | u8 hittype | u16 hitid | f32 x,y,z */
    uint8_t b[20];
    b[0] = P_WPN_SYNC;
    wput_u16(b + 1, (uint16_t)c->id);
    wput_u16(b + 3, (uint16_t)weaponid);
    b[5] = (uint8_t)hittype;
    wput_u16(b + 6, (uint16_t)hitid);
    wput_f32(b + 8, x);
    wput_f32(b + 12, y);
    wput_f32(b + 16, z);
    /* hanya yang terlihat oleh penembak (se-vw & se-interior) */
    broadcast_frame_ref(b, sizeof(b), c, c);

    lua_on_player_weapon_shot(c->id, weaponid, hittype, hitid, x, y, z);

    /* Damage actor dihitung server (otoritatif) bila kena actor. */
    if (hittype == 4) actor_damage(c->id, hitid, weaponid);
}
