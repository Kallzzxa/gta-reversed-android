/*
 * RO-MP OnPlayerKeyStateChange ala SA-MP — implementasi.
 * Lihat romp_key.h untuk desain protokol & API.
 */
#include "romp_key.h"
#include "romp_lua.h"
#include "romp_server.h"

/* Panjang frame 0x4C: 1 (type) + 2 (payload) = 3. */
/* Panjang frame 0x4D: 1 (type) + 3 (payload) = 4. */

void key_init(Client *c) {
    if (!c) return;
    c->keys = 0;
    c->last_key_ms = 0;
}

uint16_t key_get(const Client *c) {
    if (!c || !c->handshaked || c->dead) return 0;
    return c->keys;
}

/* Bangun + broadcast frame 0x4D KEY_SYNC ke semua viewer yang melihat c
 * (vw+interior scoped ala relay posisi, termasuk dirinya sendiri). */
static void key_broadcast_sync(Client *c) {
    uint8_t b[4];
    b[0] = P_KEY_SYNC;
    b[1] = (uint8_t)c->id;
    b[2] = (uint8_t)(c->keys & 0xFF);
    b[3] = (uint8_t)((c->keys >> 8) & 0xFF);
    broadcast_frame_ref(b, sizeof(b), c, NULL);
}

void key_on_state(Client *c, const uint8_t *p, size_t n) {
    if (!c || !c->handshaked || c->dead) return;
    if (!p || n != 2) return; /* payload harus pas: u16 keys LE */

    uint64_t t = now_ms();
    if (t - c->last_key_ms < KEY_MIN_MS) return; /* rate limit 20/dtk */
    c->last_key_ms = t;

    uint16_t newkeys = (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
    uint16_t oldkeys = c->keys;
    if (newkeys == oldkeys) return; /* tak ada perubahan -> diam */

    c->keys = newkeys;
    key_broadcast_sync(c);
    lua_on_player_key_change(c->id, newkeys, oldkeys);
    log_msg("[key] %s (id %d): 0x%04X -> 0x%04X",
            c->name, c->id, oldkeys, newkeys);
}
