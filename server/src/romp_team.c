/*
 * RO-MP team pemain ala SA-MP (SetPlayerTeam / GetPlayerTeam) —
 * implementasi.
 * Lihat romp_team.h untuk desain protokol & API.
 */
#include "romp_team.h"
#include "romp_lua.h"

/* Panjang frame 0x53: 1 (type) + 1 (pid) + 1 (team) = 3. */
#define TEAM_SET_LEN 3

/* Bangun frame 0x53 untuk (pid, team). Kembalikan panjang. */
static size_t team_set_frame(int pid, int team, uint8_t *b) {
    b[0] = P_TEAM_SET;
    b[1] = (uint8_t)pid;
    b[2] = (uint8_t)team;
    return TEAM_SET_LEN;
}

/* ---------------- API inti ---------------- */

int team_valid_team(int team) {
    return team >= 0 && team <= 255;
}

int team_set(Client *c, int team) {
    if (!c || !c->handshaked || c->dead) return 0;
    if (!team_valid_team(team)) return 0;

    int old = c->team;
    c->team = (uint8_t)team;

    /* Broadcast global ala SA-MP: tim terlihat semua orang (scoreboard,
     * marker, nametag berwarna per tim). */
    uint8_t b[TEAM_SET_LEN];
    broadcast_frame(b, team_set_frame(c->id, team, b), NULL);

    log_msg("[team] %s (id %d): team %d -> %d",
            c->name, c->id, old, team);
    lua_on_team_change(c->id, old, team);
    return 1;
}

int team_get(const Client *c) {
    if (!c || !c->handshaked || c->dead) return -1;
    return c->team;
}

void team_init(Client *c) {
    if (!c) return;
    c->team = TEAM_NONE;
}

void team_on_player_leave(Client *c) {
    if (!c) return;
    /* Tim bersifat per-pemain: tak ada paket yang perlu dikirim
     * (P_LEAVE sudah memberitahu semua), cukup kembalikan ke TEAM_NONE
     * agar state tak bocor bila struct dipakai ulang. */
    team_init(c);
}

void team_sync_join(Client *c) {
    if (!c || !c->handshaked || c->dead) return;
    uint8_t b[TEAM_SET_LEN];
    for (Client *t = g_clients; t; t = t->next) {
        if (!t->handshaked || t->dead) continue;
        client_send_frame(c, b,
                          team_set_frame(t->id, t->team, b));
    }
}
