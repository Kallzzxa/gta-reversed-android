/*
 * RO-MP world settings ala SA-MP — implementasi.
 * Lihat romp_world.h untuk desain protokol & API.
 */
#include "romp_world.h"
#include "romp_pworld.h" /* PWORLD_FOLLOW_GLOBAL untuk frame efektif */

/* ---- Helper frame (duplikat kecil dari romp_server.c agar modul mandiri) ---- */
static void wput_f32(uint8_t *b, float v) {
    uint32_t u; memcpy(&u, &v, 4);
    b[0] = u & 0xFF; b[1] = (u >> 8) & 0xFF;
    b[2] = (u >> 16) & 0xFF; b[3] = (u >> 24) & 0xFF;
}

/* ---------------- State global (otorsitatif server) ---------------- */
static float g_gravity = WORLD_GRAVITY_DEFAULT;
static int   g_weather = WORLD_WEATHER_DEFAULT;
static int   g_hour    = WORLD_HOUR_DEFAULT;
static int   g_minute  = WORLD_MINUTE_DEFAULT;

/* Bangun payload 0x39 WORLD_SET: f32 gravity | u8 weather | u8 hour | u8 minute.
 * Nilai EFEKTIF untuk client c: override per-pemain (lihat romp_pworld.h)
 * dipakai bila ada, global bila tidak. Gravity tetap global (SA-MP tak
 * punya override gravity per-pemain). c boleh NULL = nilai global murni. */
static size_t world_build_for(const Client *c, uint8_t *b) {
    int weather = g_weather, hour = g_hour, minute = g_minute;
    if (c) {
        if (c->pworld_weather != PWORLD_FOLLOW_GLOBAL)
            weather = c->pworld_weather;
        if (c->pworld_hour != PWORLD_FOLLOW_GLOBAL) {
            hour = c->pworld_hour;
            minute = c->pworld_minute;
        }
    }
    b[0] = P_WORLD_SET;
    wput_f32(b + 1, g_gravity);
    b[5] = (uint8_t)weather;
    b[6] = (uint8_t)hour;
    b[7] = (uint8_t)minute;
    return 8;
}

/* ---------------- API inti ---------------- */

int world_set_gravity(float g) {
    if (g < WORLD_GRAVITY_MIN || g > WORLD_GRAVITY_MAX) return 0;
    g_gravity = g;
    world_broadcast();
    log_msg("[world] gravity = %.4f", g_gravity);
    return 1;
}

int world_set_weather(int w) {
    if (w < WORLD_WEATHER_MIN || w > WORLD_WEATHER_MAX) return 0;
    g_weather = w;
    world_broadcast();
    log_msg("[world] weather = %d", g_weather);
    return 1;
}

int world_set_time(int hour, int minute) {
    if (hour < WORLD_HOUR_MIN || hour > WORLD_HOUR_MAX) return 0;
    if (minute < WORLD_MINUTE_MIN || minute > WORLD_MINUTE_MAX) return 0;
    g_hour = hour;
    g_minute = minute;
    world_broadcast();
    log_msg("[world] time = %02d:%02d", g_hour, g_minute);
    return 1;
}

float world_get_gravity(void) { return g_gravity; }
int   world_get_weather(void) { return g_weather; }
void  world_get_time(int *hour, int *minute) {
    if (hour) *hour = g_hour;
    if (minute) *minute = g_minute;
}

int world_send_to(Client *c) {
    if (!c || !c->handshaked || c->dead || c->is_npc) return 0;
    uint8_t b[8];
    /* Nilai efektif per-pemain (override-aware) — dipakai juga oleh
     * pworld_set_time/pworld_set_weather untuk satu pemain. */
    client_send_frame(c, b, world_build_for(c, b));
    return 1;
}

void world_broadcast(void) {
    /* Override-aware: pemain yang punya override menerima 0x39 efektif
     * (override mereka dipertahankan), pemain lain menerima nilai global
     * baru. Filtering sama seperti broadcast_frame (handshaked, tak mati,
     * bukan NPC). */
    for (Client *c = g_clients; c; c = c->next)
        world_send_to(c);
}

void world_init_from_config(void) {
    if (g_cfg.gravity >= WORLD_GRAVITY_MIN && g_cfg.gravity <= WORLD_GRAVITY_MAX)
        g_gravity = g_cfg.gravity;
    if (g_cfg.weather >= WORLD_WEATHER_MIN && g_cfg.weather <= WORLD_WEATHER_MAX)
        g_weather = g_cfg.weather;
    if (g_cfg.world_hour >= WORLD_HOUR_MIN && g_cfg.world_hour <= WORLD_HOUR_MAX)
        g_hour = g_cfg.world_hour;
    if (g_cfg.world_minute >= WORLD_MINUTE_MIN && g_cfg.world_minute <= WORLD_MINUTE_MAX)
        g_minute = g_cfg.world_minute;
    log_msg(" World    : gravity %.4f, weather %d, time %02d:%02d",
            g_gravity, g_weather, g_hour, g_minute);
}
