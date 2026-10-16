/*
 * RO-MP ApplyAnimation ala SA-MP — implementasi.
 * Lihat romp_anim.h untuk desain protokol & API.
 */
#include "romp_anim.h"
#include "romp_lua.h"
#include "romp_server.h"
#include <string.h>
#include <stdio.h>

void anim_init(Client *c) {
    if (!c) return;
    c->animlib[0] = '\0';
    c->animname[0] = '\0';
    c->animflags = 0;
    c->animtime = 0;
    c->animactive = 0;
}

int anim_active(const Client *c) {
    if (!c || !c->handshaked || c->dead) return 0;
    return c->animactive ? 1 : 0;
}

/* Bangun frame 0x4E ANIM_SET untuk state animasi c. return panjang frame. */
static size_t anim_frame(const Client *c, uint8_t *b) {
    size_t llen = strlen(c->animlib);
    size_t nlen = strlen(c->animname);
    size_t i = 0;
    b[i++] = P_ANIM_SET;
    b[i++] = (uint8_t)c->id;
    b[i++] = (uint8_t)llen;
    memcpy(b + i, c->animlib, llen);  i += llen;
    b[i++] = (uint8_t)nlen;
    memcpy(b + i, c->animname, nlen); i += nlen;
    b[i++] = c->animflags;
    b[i++] = (uint8_t)(c->animtime & 0xFF);
    b[i++] = (uint8_t)((c->animtime >> 8) & 0xFF);
    return i;
}

/* Bangun + broadcast 0x4E ke semua viewer yang melihat c (vw+interior
 * scoped ala relay posisi, termasuk dirinya sendiri). */
static void anim_broadcast(Client *c) {
    uint8_t b[1 + 1 + 1 + ANIM_MAX_LEN + 1 + ANIM_MAX_LEN + 1 + 2];
    size_t n = anim_frame(c, b);
    broadcast_frame_ref(b, n, c, NULL);
}

/* Kirim frame 0x4E state src ke dst (langsung, tanpa scope — pemanggil
 * sudah memfilter client_visible). */
static void anim_send_to(Client *dst, const Client *src) {
    uint8_t b[1 + 1 + 1 + ANIM_MAX_LEN + 1 + ANIM_MAX_LEN + 1 + 2];
    size_t n = anim_frame(src, b);
    if (client_send_frame(dst, b, n) < 0) dst->dead = 1;
}

/* Broadcast 0x4F ANIM_CLEAR untuk c ke viewer-nya (vw+interior scoped). */
static void anim_broadcast_clear(Client *c) {
    uint8_t b[2] = { P_ANIM_CLEAR, (uint8_t)c->id };
    broadcast_frame_ref(b, sizeof(b), c, NULL);
}

static int anim_client_ok(const Client *c) {
    return c && c->handshaked && !c->dead;
}

/* Validasi nama lib/anim: 1..ANIM_MAX_LEN byte, tanpa byte 0 di tengah. */
static int anim_name_ok(const char *s) {
    if (!s) return 0;
    size_t n = strlen(s);
    return n >= 1 && n <= ANIM_MAX_LEN;
}

int anim_apply(Client *c, const char *lib, const char *name,
               uint8_t flags, uint16_t timems) {
    if (!anim_client_ok(c)) return 0;
    if (!anim_name_ok(lib) || !anim_name_ok(name)) return 0;
    if (flags & ~(ANIM_FLAG_LOOP | ANIM_FLAG_LOCKX | ANIM_FLAG_LOCKY |
                  ANIM_FLAG_FREEZE | ANIM_FLAG_FORCESYNC)) return 0;
    /* Idempoten: state persis sama -> true tanpa broadcast ulang. */
    if (c->animactive && strcmp(c->animlib, lib) == 0 &&
        strcmp(c->animname, name) == 0 && c->animflags == flags &&
        c->animtime == timems)
        return 1;
    snprintf(c->animlib, sizeof(c->animlib), "%s", lib);
    snprintf(c->animname, sizeof(c->animname), "%s", name);
    c->animflags = flags;
    c->animtime = timems;
    c->animactive = 1;
    anim_broadcast(c);
    lua_on_player_apply_animation(c->id, c->animlib, c->animname);
    log_msg("[anim] %s (id %d): %s %s flags=0x%02X time=%u",
            c->name, c->id, c->animlib, c->animname, flags, timems);
    return 1;
}

int anim_clear(Client *c) {
    if (!anim_client_ok(c)) return 0;
    if (!c->animactive) return 0; /* bukan no-op diam: kirim false */
    c->animactive = 0;
    anim_broadcast_clear(c);
    lua_on_player_clear_animations(c->id);
    log_msg("[anim] %s (id %d): clear", c->name, c->id);
    return 1;
}

void anim_leave(Client *c) {
    if (!c) return;
    anim_init(c); /* P_LEAVE sudah menghapus ped di semua client */
}

void anim_sync_join(Client *dst) {
    if (!anim_client_ok(dst)) return;
    for (Client *o = g_clients; o; o = o->next) {
        if (o == dst || !o->animactive) continue;
        if (!anim_client_ok(o)) continue;
        if (!client_visible(dst, o)) continue;
        anim_send_to(dst, o);
    }
}

void anim_on_world_change(Client *c) {
    if (!anim_client_ok(c) || !c->animactive) return;
    /* State tak perlu tahu vw lama — kirim ulang 0x4E: viewer baru
     * menerapkannya, viewer lama tak terkena (scope sudah vw baru).
     * Untuk viewer lama yang kini tak melihat, kirim 0x4F eksplisit. */
    uint8_t cb[2] = { P_ANIM_CLEAR, (uint8_t)c->id };
    for (Client *o = g_clients; o; o = o->next) {
        if (o == c || !o->handshaked || o->dead || o->is_npc) continue;
        if (!client_visible(c, o)) {
            if (client_send_frame(o, cb, sizeof(cb)) < 0) o->dead = 1;
        }
    }
    anim_broadcast(c); /* 0x4E ke viewer vw+interior baru */
}
