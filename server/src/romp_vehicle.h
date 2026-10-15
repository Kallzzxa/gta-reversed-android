/*
 * RO-MP vehicle sync ala SA-MP — registry kendaraan server-side.
 *
 * Server bersifat otoritatif untuk daftar kendaraan (spawn/destroy/kursi),
 * sedangkan posisi kendaraan di-relay dari client yang jadi sopir
 * (paket 0x12 VEH -> broadcast 0x13 VEH_SYNC), mirip relay posisi pemain
 * 0x10 -> 0x11. Health/damage ikut di paket sync; health 0 = hancur
 * (event onVehicleDeath + respawn otomatis bila respawn_ms > 0).
 *
 * Kursi: 0 = sopir, 1..3 = penumpang.
 */
#pragma once

#include "romp_server.h"

#define VEH_SEATS      4          /* 0 = sopir, 1..3 = penumpang */
#define VEH_MAX        1000       /* maks kendaraan aktif */
#define VEH_HEALTH_MAX 1000      /* skala health ala SA-MP (<=250 = hancur) */
#define VEH_HEALTH_WRECK 250
#define VEH_COMP_SLOTS 16        /* slot komponen 0..13 standar SA-MP
                                  * (0 spoiler, 1 hood, 2 roof, 3 sideskirt,
                                  *  4 lamps, 5 nitro, 6 exhaust, 7 wheels,
                                  *  8 stereo, 9 hydraulics, 10 front bumper,
                                  *  11 rear bumper, 12 vent right, 13 vent left)
                                  *  + 14/15 front/rear bullbar (ekstensi vSync) */
#define VEH_PAINTJOB_NONE 255    /* paintjob = tak ada (ala SA-MP) */

typedef struct Vehicle {
    int      id;                 /* vehicleid, mulai dari 1 */
    uint16_t model;              /* id model SA-MP (400-611) */
    float    sx, sy, sz, sangle; /* posisi spawn */
    float    x, y, z, heading;   /* posisi terkini */
    uint8_t  c1, c2;             /* warna */
    uint16_t health;             /* 0..1000 */
    int      driver;             /* player id sopir, -1 = kosong */
    int      pass[3];            /* player id penumpang seat 1..3, -1 = kosong */
    uint32_t respawn_ms;         /* 0 = tidak respawn otomatis */
    uint32_t vw;                 /* virtual world ala SA-MP (default 0) */
    uint16_t comp[VEH_COMP_SLOTS]; /* compid per slot (ala AddVehicleComponent),
                                    * 0 = slot kosong */
    uint8_t  paintjob;           /* 0..2, 255 = tak ada (ala SA-MP) */
    uint64_t dead_at;            /* now_ms() saat hancur */
    int      destroyed;          /* 1 = sedang hancur, menunggu respawn */
    uint64_t last_sync_ms;       /* rate limit paket VEH */
    struct Vehicle *next;
} Vehicle;

/* ---- Registry ---- */
Vehicle *vehicle_by_id(int id);
Vehicle *vehicle_first(void);       /* iterasi: NULL = tidak ada */
Vehicle *vehicle_next(Vehicle *v);
int      vehicle_count(void);
/* Buat kendaraan. model 400..611, respawn_ms 0 = tanpa respawn otomatis.
 * Kembalikan vehicleid (>0) atau -1 bila gagal. Broadcast 0x14. */
int      vehicle_create(uint16_t model, float x, float y, float z, float angle,
                        uint8_t c1, uint8_t c2, uint32_t respawn_ms);
/* Hancurkan kendaraan (keluarkan penumpang, broadcast 0x15). 1 = sukses. */
int      vehicle_destroy(int id);

/* ---- State ---- */
/* Set posisi server-side (mis. teleport). Broadcast 0x13 driverless. */
int vehicle_set_pos(int id, float x, float y, float z, float angle);
/* Set health 0..1000. Memicu damage/death/spawn event bila berubah. */
int vehicle_set_health(int id, uint16_t health, int killerid);
/* Kurangi health sebesar amount. Kembalikan health baru, -1 bila gagal. */
int vehicle_damage(int id, uint16_t amount, int killerid);
/* Kembalikan ke posisi spawn + health penuh (penumpang tetap). */
int vehicle_respawn(int id);
int vehicle_set_colors(int id, uint8_t c1, uint8_t c2);
/* Pindahkan kendaraan ke virtual world lain (SetVehicleVirtualWorld).
 * Broadcast despawn ke vw lama + spawn ke vw baru. 1 = sukses. */
int vehicle_set_vw(int id, uint32_t vw);

/* ---- Komponen kendaraan ala SA-MP (tuning) ---- */
/* Slot komponen 0..15 untuk compid, -1 bila compid invalid/tak ada data. */
int vehicle_component_type(uint16_t compid);
/* Pasang komponen (ala AddVehicleComponent). Komponen dengan compid invalid
 * ditolak. Menimpa komponen yang sudah ada di slot yang sama (seperti SA-MP).
 * Broadcast 0x3A ke vw kendaraan. 1 = sukses. */
int vehicle_add_component(int vid, uint16_t compid);
/* Lepas komponen (ala RemoveVehicleComponent). Broadcast 0x3B. 1 = sukses. */
int vehicle_remove_component(int vid, uint16_t compid);
/* compid di slot (0..15), -1 bila slot kosong/vid/slot invalid. */
int vehicle_get_component_in_slot(int vid, int slot);
/* Ganti paintjob ala SA-MP (ChangeVehiclePaintjob): 0..2, 255 = lepas.
 * Broadcast 0x3C. 1 = sukses. */
int vehicle_set_paintjob(int vid, uint8_t paintjob);
/* Paintjob kendaraan (0..2 | 255 = tak ada), -1 bila vid invalid. */
int vehicle_get_paintjob(int vid);

/* ---- Penumpang ---- */
/* Masukkan pemain ke kendaraan (seat 0..3). Broadcast 0x16 + event
 * onPlayerEnterVehicle. 1 = sukses. */
int vehicle_put_player(int pid, int vid, int seat);
/* Keluarkan pemain dari kendaraannya. Broadcast 0x16 + event
 * onPlayerExitVehicle. 1 = sukses. */
int vehicle_remove_player(int pid);
/* Versi diam (tanpa event Lua): dipakai saat pemain disconnect. */
void vehicle_remove_player_silent(int pid);
/* vehicleid pemain, atau -1 bila tidak di kendaraan. */
int vehicle_of_player(int pid);
/* Kursi pemain (0..3), atau -1 bila tidak di kendaraan. */
int vehicle_seat_of_player(int pid);
/* Player id penghuni seat (0..3), atau -1 bila kosong/invalid. */
int vehicle_occupant(int vid, int seat);

/* ---- Sync jaringan ---- */
/* Proses paket 0x12 VEH dari client (harus sopir). Relay 0x13. */
void vehicle_on_packet(Client *c, const uint8_t *p, size_t n);
/* Kirim 0x14 (+ 0x16 untuk kursi terisi) semua kendaraan ke client baru
 * (hanya yang vw-nya sama dengan pemain). */
void vehicle_sync_join(Client *c);
/* Resync kendaraan saat pemain pindah vw (dipakai player_set_world):
 * despawn kendaraan vw lama + spawn kendaraan vw baru ke client. */
void vehicle_sync_world(Client *c, uint32_t old_vw);
/* Dipanggil tiap 1 detik dari main loop: respawn kendaraan hancur. */
void vehicle_tick(uint64_t now);
