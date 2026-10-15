/*
 * RO-MP wanted level ala SA-MP (SetPlayerWantedLevel / GetPlayerWantedLevel)
 * — implementasi.
 * Lihat romp_wanted.h untuk desain protokol & API.
 */
#include "romp_wanted.h"
#include "romp_lua.h"

/* Panjang frame 0x48: 1 (type) + 1 (pid) + 1 (level) = 3. */
#define WANTED_SET_LEN 3

/* Bangun frame 0x48 untuk (pid, level). Kembalikan panjang. */
static size_t wanted_set_frame(int pid, int level, uint8_t *b) {
    b[0] = P_WANTED_SET;
    b[1] = (uint8_t)pid;
    b[2] = (uint8_t)level;
    return WANTED_SET_LEN;
}

/* ---------------- API inti ---------------- */

int wanted_valid_level(int level) {
    return level >= WANTED_LEVEL_MIN && level <= WANTED_LEVEL_MAX;
}

int wanted_set(Client *c, int level) {
    if (!c || !c->handshaked || c->dead) return 0;
    if (!wanted_valid_level(level)) return 0;

    int old = c->wanted_level;
    c->wanted_level = (uint8_t)level;

    /* Broadcast global ala SA-MP: semua orang melihat wanted pemain ini. */
    uint8_t b[WANTED_SET_LEN];
    broadcast_frame(b, wanted_set_frame(c->id, level, b), NULL);

    log_msg("[wanted] %s (id %d): wanted %d -> %d",
            c->name, c->id, old, level);
    lua_on_wanted_level_change(c->id, old, level);
    return 1;
}

int wanted_get(const Client *c) {
    if (!c || !c->handshaked || c->dead) return -1;
    return c->wanted_level;
}

void wanted_init(Client *c) {
    if (!c) return;
    c->wanted_level = 0;
}

void wanted_on_player_leave(Client *c) {
    if (!c) return;
    /* Wanted bersifat per-pemain: tak ada paket yang perlu dikirim
     * (P_LEAVE sudah memberitahu semua), cukup kembalikan ke 0 agar
     * state tak bocor bila struct dipakai ulang. */
    wanted_init(c);
}

void wanted_sync_join(Client *c) {
    if (!c || !c->handshaked || c->dead) return;
    uint8_t b[WANTED_SET_LEN];
    for (Client *t = g_clients; t; t = t->next) {
        if (!t->handshaked || t->dead) continue;
        client_send_frame(c, b,
                          wanted_set_frame(t->id, t->wanted_level, b));
    }
}
