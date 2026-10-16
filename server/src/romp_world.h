/*
 * RO-MP world settings ala SA-MP (SetGravity/SetWeather/SetWorldTime) — server.
 *
 * State dunia bersifat global (sama untuk semua pemain, seperti di SA-MP)
 * dan otoritatif di server. Setiap perubahan di-broadcast ke semua client
 * secara override-aware (lihat romp_pworld.h): pemain yang punya override
 * waktu/cuaca menerima nilai efektifnya, yang lain menerima nilai global.
 * Dikirim ulang (efektif) ke pemain yang baru join di do_join.
 *
 * Protokol (server -> client, lihat PROTOCOL.md):
 *   0x39 WORLD_SET: f32 gravity | u8 weather | u8 hour | u8 minute
 *     (nilai efektif per-pemain: override bila ada, global bila tidak)
 *
 * Default ala GTA:SA/SA-MP: gravity 0.008, weather 10 (Sunny), jam 12:00.
 * Nilai awal bisa dioverride dari server.cfg: `gravity`, `weather`,
 * `worldtime HH:MM`.
 */
#pragma once

#include "romp_server.h"

/* Batas validasi ala SA-MP (fisis GTA:SA). */
#define WORLD_GRAVITY_MIN   0.001f
#define WORLD_GRAVITY_MAX   0.5f
#define WORLD_WEATHER_MIN   0
#define WORLD_WEATHER_MAX   255
#define WORLD_HOUR_MIN      0
#define WORLD_HOUR_MAX      23
#define WORLD_MINUTE_MIN    0
#define WORLD_MINUTE_MAX    59

#define WORLD_GRAVITY_DEFAULT 0.008f
#define WORLD_WEATHER_DEFAULT 10
#define WORLD_HOUR_DEFAULT    12
#define WORLD_MINUTE_DEFAULT  0

/* Set gravity (ala SA-MP SetGravity). Return 1 bila valid & di-broadcast,
 * 0 bila nilai di luar rentang (state tak berubah, tak ada broadcast). */
int world_set_gravity(float g);

/* Set weather id (ala SA-MP SetWeather). Return 1/0 seperti di atas. */
int world_set_weather(int w);

/* Set jam dunia (ala SA-MP SetWorldTime + menit). Return 1/0. */
int world_set_time(int hour, int minute);

/* Getter state saat ini (otorsatif server). */
float world_get_gravity(void);
int   world_get_weather(void);
void  world_get_time(int *hour, int *minute);

/* Kirim state ke satu client (resync saat join). Return 1 bila terkirim,
 * 0 bila client tak valid/NPC/sudah mati. */
int world_send_to(Client *c);

/* Broadcast state ke semua client secara override-aware (lihat romp_pworld.h):
 * pemain dengan override waktu/cuaca menerima 0x39 efektif, yang lain
 * menerima nilai global baru. */
void world_broadcast(void);

/* Inisialisasi state dari server.cfg (dipanggil sekali saat startup,
 * setelah cfg_load; sebelum lua_init agar gamemode melihat nilai config). */
void world_init_from_config(void);
