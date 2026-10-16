/*
 * RO-MP override dunia per-pemain ala SA-MP — implementasi.
 * Lihat romp_pworld.h untuk desain protokol & API.
 */
#include "romp_pworld.h"
#include "romp_world.h"

/* Panjang frame 0x54: 1 (type) + 1 (pid) + 1 (clock_enabled) = 3. */
#define CLOCK_SET_LEN 3

/* Bangun frame 0x54 untuk (pid, on). Kembalikan panjang. */
static size_t clock_set_frame(int pid, int on, uint8_t *b) {
    b[0] = P_CLOCK_SET;
    b[1] = (uint8_t)pid;
    b[2] = (uint8_t)(on ? 1 : 0);
    return CLOCK_SET_LEN;
}

/* ---------------- API inti ---------------- */

void pworld_init(Client *c) {
    if (!c) return;
    c->pworld_hour    = PWORLD_FOLLOW_GLOBAL;
    c->pworld_minute  = 0;
    c->pworld_weather = PWORLD_FOLLOW_GLOBAL;
    c->pworld_clock   = 1;
}

void pworld_on_player_leave(Client *c) {
    if (!c) return;
    /* Per-pemain: tak ada paket (P_LEAVE sudah broadcast), cukup reset
     * agar state tak bocor bila struct dipakai ulang. */
    pworld_init(c);
}

static int pworld_valid(Client *c) {
    return c && c->handshaked && !c->dead;
}

int pworld_set_time(Client *c, int hour, int minute) {
    if (!pworld_valid(c)) return 0;
    if (hour < WORLD_HOUR_MIN || hour > WORLD_HOUR_MAX) return 0;
    if (minute < WORLD_MINUTE_MIN || minute > WORLD_MINUTE_MAX) return 0;
    c->pworld_hour   = hour;
    c->pworld_minute = minute;
    pworld_send_effective(c);
    log_msg("[pworld] %s (id %d): override time %02d:%02d",
            c->name, c->id, hour, minute);
    return 1;
}

int pworld_get_time(const Client *c, int *hour, int *minute, int *is_override) {
    if (!pworld_valid((Client *)c)) return 0;
    int ov = c->pworld_hour != PWORLD_FOLLOW_GLOBAL;
    int h, m;
    if (ov) { h = c->pworld_hour; m = c->pworld_minute; }
    else world_get_time(&h, &m);
    if (hour) *hour = h;
    if (minute) *minute = m;
    if (is_override) *is_override = ov;
    return 1;
}

int pworld_set_weather(Client *c, int weather) {
    if (!pworld_valid(c)) return 0;
    if (weather < WORLD_WEATHER_MIN || weather > WORLD_WEATHER_MAX) return 0;
    c->pworld_weather = weather;
    pworld_send_effective(c);
    log_msg("[pworld] %s (id %d): override weather %d",
            c->name, c->id, weather);
    return 1;
}

int pworld_get_weather(const Client *c, int *weather, int *is_override) {
    if (!pworld_valid((Client *)c)) return 0;
    int ov = c->pworld_weather != PWORLD_FOLLOW_GLOBAL;
    int w = ov ? c->pworld_weather : world_get_weather();
    if (weather) *weather = w;
    if (is_override) *is_override = ov;
    return 1;
}

int pworld_toggle_clock(Client *c, int on) {
    if (!pworld_valid(c)) return 0;
    c->pworld_clock = (uint8_t)(on ? 1 : 0);
    /* Hanya ke pemain itu sendiri (no-op untuk NPC tanpa socket). */
    if (!c->is_npc) {
        uint8_t b[CLOCK_SET_LEN];
        client_send_frame(c, b, clock_set_frame(c->id, c->pworld_clock, b));
    }
    log_msg("[pworld] %s (id %d): clock %s",
            c->name, c->id, c->pworld_clock ? "jalan" : "dibekukan");
    return 1;
}

int pworld_get_clock(const Client *c) {
    if (!pworld_valid((Client *)c)) return -1;
    return c->pworld_clock ? 1 : 0;
}

int pworld_send_effective(Client *c) {
    /* world_send_to sudah override-aware (lihat romp_world.c). */
    return world_send_to(c);
}

void pworld_sync_join(Client *c) {
    if (!pworld_valid(c)) return;
    /* Resync state jam ke joiner (0x39 efektif sudah dikirim
     * world_send_to di do_join). No-op untuk NPC. */
    if (c->is_npc) return;
    uint8_t b[CLOCK_SET_LEN];
    client_send_frame(c, b, clock_set_frame(c->id, c->pworld_clock, b));
}
