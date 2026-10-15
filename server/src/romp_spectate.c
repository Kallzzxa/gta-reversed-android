/*
 * RO-MP spectate ala SA-MP (TogglePlayerSpectating / PlayerSpectatePlayer /
 * PlayerSpectateVehicle) — implementasi.
 * Lihat romp_spectate.h untuk desain protokol & API.
 */
#include "romp_spectate.h"
#include "romp_lua.h"
#include "romp_vehicle.h"

/* Panjang frame 0x4B: 1 (type) + 6 (payload) = 7. */
#define SPECTATE_FRAME_LEN 7

int spectate_valid_mode(int mode) {
    return mode >= SPECTATE_MODE_NORMAL && mode <= SPECTATE_MODE_SIDE;
}

void spectate_init(Client *c) {
    if (!c) return;
    c->spectating = 0;
    c->spectate_type = SPECTATE_TARGET_NONE;
    c->spectate_target = 0;
    c->spectate_mode = SPECTATE_MODE_NORMAL;
}

int spectate_is(const Client *c) {
    return c && c->handshaked && !c->dead && c->spectating;
}

int spectate_get(const Client *c, int *type, int *target, int *mode) {
    if (!spectate_is(c)) return 0;
    if (type) *type = c->spectate_type;
    if (target) *target = c->spectate_target;
    if (mode) *mode = c->spectate_mode;
    return 1;
}

/* Bangun frame 0x4B untuk state terkini c. */
static size_t spectate_frame(const Client *c, uint8_t *b) {
    b[0] = P_SPECTATE;
    b[1] = (uint8_t)c->id;
    b[2] = c->spectating ? 1 : 0;
    b[3] = c->spectate_type;
    b[4] = (uint8_t)(c->spectate_target & 0xFF);
    b[5] = (uint8_t)((c->spectate_target >> 8) & 0xFF);
    b[6] = c->spectate_mode;
    return SPECTATE_FRAME_LEN; /* 1 type + 6 payload */
}

void spectate_send_to(Client *dst, const Client *spec) {
    if (!dst || !dst->handshaked || dst->dead) return;
    if (!spec) return;
    uint8_t b[8];
    size_t n = spectate_frame(spec, b);
    client_send_frame(dst, b, n);
}

void spectate_broadcast(Client *c) {
    if (!c || !c->handshaked || c->dead) return;
    uint8_t b[8];
    size_t n = spectate_frame(c, b);
    /* vw-scoped ke vw penonton (termasuk dirinya sendiri -> except NULL)
     * ala paket entity lain; interior ikut di-scope via client_visible. */
    broadcast_frame_ref(b, n, c, NULL);
    log_msg("[spectate] %s (id %d): %s type=%d target=%d mode=%d",
            c->name, c->id, c->spectating ? "ON" : "OFF",
            c->spectate_type, c->spectate_target, c->spectate_mode);
}

/* Terapkan state baru + broadcast + event. Dipakai semua path perubahan. */
static void spectate_apply(Client *c, int on, int type, int target, int mode) {
    c->spectating = on ? 1 : 0;
    c->spectate_type = (uint8_t)type;
    c->spectate_target = (uint16_t)target;
    c->spectate_mode = (uint8_t)mode;
    spectate_broadcast(c);
    lua_on_player_spectate(c->id, c->spectating, c->spectate_type,
                           c->spectate_target, c->spectate_mode);
}

static int spectate_client_ok(const Client *c) {
    return c && c->handshaked && !c->dead;
}

int spectate_toggle(Client *c, int on) {
    if (!spectate_client_ok(c)) return 0;
    if (c->is_npc) return 0; /* NPC tak punya kamera -> tak bisa spectate */
    if (on && c->spectating) return 1;   /* idempoten */
    if (!on && !c->spectating) return 1; /* idempoten */
    if (on)
        spectate_apply(c, 1, c->spectate_type, c->spectate_target,
                       c->spectate_mode);
    else
        spectate_apply(c, 0, SPECTATE_TARGET_NONE, 0, SPECTATE_MODE_NORMAL);
    return 1;
}

int spectate_stop(Client *c) {
    if (!spectate_client_ok(c)) return 0;
    if (!c->spectating) return 0;
    spectate_apply(c, 0, SPECTATE_TARGET_NONE, 0, SPECTATE_MODE_NORMAL);
    return 1;
}

/* Validasi relasi spectate milik s masih masuk akal: target ada & se-vw. */
static int spectate_relation_valid(const Client *s) {
    if (!s->spectating) return 1;
    if (s->spectate_type == SPECTATE_TARGET_PLAYER) {
        Client *t = client_by_id(s->spectate_target);
        if (!spectate_client_ok(t)) return 0;
        if (t->vw != s->vw) return 0;
        return 1;
    }
    if (s->spectate_type == SPECTATE_TARGET_VEHICLE) {
        Vehicle *v = vehicle_by_id(s->spectate_target);
        if (!v || v->destroyed) return 0;
        if (v->vw != s->vw) return 0;
        return 1;
    }
    return 1; /* type NONE: spectate tanpa target selalu valid */
}

int spectate_player(Client *c, int target_pid, int mode) {
    if (!spectate_client_ok(c)) return 0;
    if (c->is_npc) return 0; /* NPC tak bisa spectate */
    if (!spectate_valid_mode(mode)) return 0;
    Client *t = client_by_id(target_pid);
    if (!spectate_client_ok(t)) return 0;
    if (t == c) return 0; /* tak bisa spectate diri sendiri */
    if (t->vw != c->vw) return 0; /* stream posisi target hanya se-vw */
    if (c->spectating && c->spectate_type == SPECTATE_TARGET_PLAYER &&
        c->spectate_target == (uint16_t)target_pid &&
        c->spectate_mode == (uint8_t)mode)
        return 1; /* idempoten */
    spectate_apply(c, 1, SPECTATE_TARGET_PLAYER, target_pid, mode);
    return 1;
}

int spectate_vehicle(Client *c, int vid, int mode) {
    if (!spectate_client_ok(c)) return 0;
    if (c->is_npc) return 0; /* NPC tak bisa spectate */
    if (!spectate_valid_mode(mode)) return 0;
    Vehicle *v = vehicle_by_id(vid);
    if (!v || v->destroyed) return 0;
    if (v->vw != c->vw) return 0; /* stream posisi kendaraan hanya se-vw */
    if (c->spectating && c->spectate_type == SPECTATE_TARGET_VEHICLE &&
        c->spectate_target == (uint16_t)vid &&
        c->spectate_mode == (uint8_t)mode)
        return 1; /* idempoten */
    spectate_apply(c, 1, SPECTATE_TARGET_VEHICLE, vid, mode);
    return 1;
}

void spectate_sync_join(Client *c) {
    if (!spectate_client_ok(c)) return;
    for (Client *o = g_clients; o; o = o->next) {
        if (o == c || !o->spectating) continue;
        if (!client_visible(c, o)) continue;
        spectate_send_to(c, o);
    }
}

void spectate_on_player_leave(Client *c) {
    if (!c) return;
    /* 1. Penonton yang disconnect: hentikan diam-diam (tanpa broadcast —
     * P_LEAVE yang dikirim do_leave sudah menghapus ped di semua client). */
    spectate_init(c);
    /* 2. Penonton lain yang menarget pemain ini: stop paksa. */
    for (Client *o = g_clients; o; o = o->next) {
        if (!o->spectating) continue;
        if (o->spectate_type == SPECTATE_TARGET_PLAYER &&
            o->spectate_target == (uint16_t)c->id)
            spectate_stop(o);
    }
}

void spectate_on_vehicle_destroy(int vid) {
    for (Client *o = g_clients; o; o = o->next) {
        if (!o->spectating) continue;
        if (o->spectate_type == SPECTATE_TARGET_VEHICLE &&
            o->spectate_target == (uint16_t)vid)
            spectate_stop(o);
    }
}

void spectate_on_world_change(void) {
    for (Client *o = g_clients; o; o = o->next) {
        if (!o->spectating) continue;
        if (!spectate_relation_valid(o))
            spectate_stop(o);
    }
}

void spectate_on_vehicle_world_change(int vid) {
    for (Client *o = g_clients; o; o = o->next) {
        if (!o->spectating) continue;
        if (o->spectate_type != SPECTATE_TARGET_VEHICLE) continue;
        if (o->spectate_target != (uint16_t)vid) continue;
        Vehicle *v = vehicle_by_id(vid);
        if (!v || v->destroyed || v->vw != o->vw)
            spectate_stop(o);
    }
}
