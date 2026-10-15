/*
 * RO-MP attached object ala SA-MP (SetPlayerAttachedObject/
 * RemovePlayerAttachedObject) — implementasi.
 * Lihat romp_attach.h untuk desain protokol & API.
 */
#include "romp_attach.h"

/* ---- Helper frame (duplikat kecil dari romp_server.c agar modul mandiri) ---- */
static void aput_u32(uint8_t *b, uint32_t v) {
    b[0] = v & 0xFF; b[1] = (v >> 8) & 0xFF;
    b[2] = (v >> 16) & 0xFF; b[3] = (v >> 24) & 0xFF;
}

/* Panjang payload 0x3F: set = 44 byte, remove = 7 byte. */
#define ATTACH_SET_LEN    44
#define ATTACH_REMOVE_LEN 7

/* Bangun frame set untuk slot `index` milik `owner`. Kembalikan panjang. */
static size_t attach_set_frame(const Client *owner, int index, uint8_t *b) {
    const AttachSlot *s = &owner->attach[index];
    size_t n = 0;
    b[n++] = P_ATTACH_OBJ;
    b[n++] = (uint8_t)owner->id;
    b[n++] = (uint8_t)index;
    aput_u32(b + n, s->modelid); n += 4;
    b[n++] = s->bone;
    for (int i = 0; i < 3; i++) {
        uint32_t u; memcpy(&u, &s->off[i], 4);
        b[n++] = u & 0xFF; b[n++] = (u >> 8) & 0xFF;
        b[n++] = (u >> 16) & 0xFF; b[n++] = (u >> 24) & 0xFF;
    }
    for (int i = 0; i < 3; i++) {
        uint32_t u; memcpy(&u, &s->rot[i], 4);
        b[n++] = u & 0xFF; b[n++] = (u >> 8) & 0xFF;
        b[n++] = (u >> 16) & 0xFF; b[n++] = (u >> 24) & 0xFF;
    }
    for (int i = 0; i < 3; i++) {
        uint32_t u; memcpy(&u, &s->scale[i], 4);
        b[n++] = u & 0xFF; b[n++] = (u >> 8) & 0xFF;
        b[n++] = (u >> 16) & 0xFF; b[n++] = (u >> 24) & 0xFF;
    }
    return n; /* 44 */
}

/* Bangun frame remove pendek (7 byte). */
static size_t attach_remove_frame(int pid, int index, uint8_t *b) {
    b[0] = P_ATTACH_OBJ;
    b[1] = (uint8_t)pid;
    b[2] = (uint8_t)index;
    aput_u32(b + 3, 0); /* modelid = 0 -> remove */
    return ATTACH_REMOVE_LEN;
}

static int valid_set_args(const Client *c, int index, uint32_t modelid,
                          uint8_t bone, const float scale[3]) {
    if (!c || !c->handshaked || c->dead) return 0;
    if (index < 0 || index >= ATTACH_MAX_SLOTS) return 0;
    if (modelid == 0) return 0; /* 0 = invalid (dipakai frame remove) */
    if (bone < ATTACH_BONE_MIN || bone > ATTACH_BONE_MAX) return 0;
    if (!scale || scale[0] <= 0.0f || scale[1] <= 0.0f || scale[2] <= 0.0f)
        return 0;
    return 1;
}

/* ---------------- API inti ---------------- */

int attach_set(Client *c, int index, uint32_t modelid, uint8_t bone,
               const float off[3], const float rot[3], const float scale[3]) {
    if (!valid_set_args(c, index, modelid, bone, scale)) return 0;

    /* Simpan state otoritatif (slot yang sama ditimpa, ala SA-MP). */
    AttachSlot *s = &c->attach[index];
    s->used = 1;
    s->modelid = modelid;
    s->bone = bone;
    memcpy(s->off, off, sizeof(s->off));
    memcpy(s->rot, rot, sizeof(s->rot));
    memcpy(s->scale, scale, sizeof(s->scale));

    /* Broadcast vw-scoped: semua pemain di vw pemilik, termasuk pemilik
     * sendiri (dia juga perlu me-render attach-nya). NPC pemilik tak
     * punya socket — broadcast_frame_vw otomatis melewatinya. */
    uint8_t b[ATTACH_SET_LEN];
    broadcast_frame_vw(b, attach_set_frame(c, index, b), NULL, c->vw);

    log_msg("[attach] %s (id %d): set slot %d model %u bone %d",
            c->name, c->id, index, modelid, bone);
    return 1;
}

int attach_remove(Client *c, int index) {
    if (!c || !c->handshaked || c->dead) return 0;
    if (index < 0 || index >= ATTACH_MAX_SLOTS) return 0;
    AttachSlot *s = &c->attach[index];
    if (!s->used) return 0; /* ala SA-MP: remove slot kosong = gagal */
    memset(s, 0, sizeof(*s));

    uint8_t b[ATTACH_REMOVE_LEN];
    broadcast_frame_vw(b, attach_remove_frame(c->id, index, b), NULL, c->vw);

    log_msg("[attach] %s (id %d): remove slot %d", c->name, c->id, index);
    return 1;
}

int attach_slot_used(Client *c, int index) {
    if (!c || index < 0 || index >= ATTACH_MAX_SLOTS) return 0;
    return c->attach[index].used;
}

void attach_sync_join(Client *c) {
    uint8_t b[ATTACH_SET_LEN];
    for (Client *o = g_clients; o; o = o->next) {
        if (!o->handshaked || o->dead || o == c) continue;
        if (o->vw != c->vw) continue; /* vw-scoped */
        for (int i = 0; i < ATTACH_MAX_SLOTS; i++) {
            if (!o->attach[i].used) continue;
            client_send_frame(c, b, attach_set_frame(o, i, b));
        }
    }
}

void attach_sync_world(Client *c, uint32_t old_vw) {
    uint8_t b[ATTACH_SET_LEN];
    if (old_vw != c->vw) {
        /* attach milik pemain vw lama tak terlihat lagi -> remove */
        for (Client *o = g_clients; o; o = o->next) {
            if (!o->handshaked || o->dead || o == c) continue;
            if (o->vw != old_vw) continue;
            for (int i = 0; i < ATTACH_MAX_SLOTS; i++) {
                if (!o->attach[i].used) continue;
                client_send_frame(c, b, attach_remove_frame(o->id, i, b));
            }
        }
    }
    if (old_vw == c->vw) return;
    /* attach milik pemain vw baru -> set */
    for (Client *o = g_clients; o; o = o->next) {
        if (!o->handshaked || o->dead || o == c) continue;
        if (o->vw != c->vw) continue;
        for (int i = 0; i < ATTACH_MAX_SLOTS; i++) {
            if (!o->attach[i].used) continue;
            client_send_frame(c, b, attach_set_frame(o, i, b));
        }
    }
}

void attach_on_player_leave(Client *c) {
    if (!c) return;
    memset(c->attach, 0, sizeof(c->attach));
}
