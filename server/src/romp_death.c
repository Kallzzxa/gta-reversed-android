/*
 * RO-MP kill feed ala SA-MP (SendDeathMessage / OnPlayerDeath) — implementasi.
 * Lihat romp_death.h untuk desain protokol & API.
 */
#include "romp_death.h"
#include "romp_lua.h"

#include <string.h>

/* Validasi killerid: 255 (DEATH_KILLER_NONE) atau pemain yang connect. */
static Client *death_valid_killer(int killerid) {
    if (killerid == DEATH_KILLER_NONE) return NULL;
    if (killerid < 0 || killerid > 254) return NULL;
    Client *k = client_by_id(killerid);
    if (!k || !k->handshaked || k->dead) return NULL;
    return k;
}

/* Bangun + broadcast frame 0x47 DEATH_MSG ke semua pemain (global).
 * Dipakai baik untuk laporan client maupun sendDeathMessage manual. */
static void death_broadcast(int killerid, int victimid, int reason) {
    uint8_t b[4];
    b[0] = P_DEATH_MSG;
    b[1] = (uint8_t)killerid;
    b[2] = (uint8_t)victimid;
    b[3] = (uint8_t)reason;
    broadcast_frame(b, sizeof(b), NULL);
}

void death_on_report(Client *c, const uint8_t *p, size_t n) {
    if (!c || !c->handshaked || c->dead) return;
    if (n != 2 || !p) return; /* payload harus pas: killerid + reason */

    int killerid = p[0];
    int reason   = p[1];

    if (!death_valid_killer(killerid)) {
        /* killerid 255 lolos sebagai "tak ada pembunuh"; id tak valid
         * lainnya (mis. pemain yang tidak connect) -> buang. */
        if (killerid != DEATH_KILLER_NONE) return;
    }
    if (reason < 0 || reason > DEATH_REASON_MAX) return;

    int victimid = c->id;
    death_broadcast(killerid, victimid, reason);
    log_msg("[death] %s (id %d) mati — killer=%d reason=%d",
            c->name, victimid, killerid, reason);
    lua_on_player_death(victimid, killerid, reason);
}

int death_send(int killerid, int victimid, int reason) {
    if (victimid < 0 || victimid > 254) return 0;
    Client *v = client_by_id(victimid);
    if (!v || !v->handshaked || v->dead) return 0;
    if (!death_valid_killer(killerid) && killerid != DEATH_KILLER_NONE)
        return 0;
    if (reason < 0 || reason > DEATH_REASON_MAX) return 0;

    death_broadcast(killerid, victimid, reason);
    log_msg("[death] manual: killer=%d victim=%d reason=%d",
            killerid, victimid, reason);
    return 1;
}
