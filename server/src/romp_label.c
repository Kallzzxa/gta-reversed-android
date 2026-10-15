/*
 * RO-MP 3D text label ala SA-MP — implementasi registry label.
 * Lihat romp_label.h untuk desain protokol & API.
 */
#include "romp_label.h"
#include "romp_vehicle.h"

static Label3D *g_labels = NULL;

#define LABEL_DRAW_DEFAULT 100.0f

/* ---- Helper frame (duplikat kecil dari romp_server.c agar modul mandiri) ---- */
static void lput_u16(uint8_t *b, uint16_t v) { b[0] = v & 0xFF; b[1] = (v >> 8) & 0xFF; }
static void lput_u32(uint8_t *b, uint32_t v) {
    lput_u16(b, (uint16_t)(v & 0xFFFF));
    lput_u16(b + 2, (uint16_t)(v >> 16));
}
static void lput_f32(uint8_t *b, float v) {
    uint32_t u; memcpy(&u, &v, 4);
    lput_u16(b, (uint16_t)(u & 0xFFFF));
    lput_u16(b + 2, (uint16_t)(u >> 16));
}

/* ---------------- Registry ---------------- */

Label3D *label_by_id(int id) {
    if (id <= 0) return NULL;
    for (Label3D *l = g_labels; l; l = l->next)
        if (l->id == id) return l;
    return NULL;
}

Label3D *label_first(void) { return g_labels; }

int label_count(void) {
    int n = 0;
    for (Label3D *l = g_labels; l; l = l->next) n++;
    return n;
}

static int alloc_lid(void) {
    for (int id = 1; id <= LABEL_MAX; id++)
        if (!label_by_id(id)) return id;
    return -1;
}

/* Virtual world efektif: vw entity bila di-attach, vw sendiri bila statis. */
uint32_t label_eff_vw(Label3D *l) {
    if (!l) return 0;
    if (l->attach_type == LABEL_ATTACH_PLAYER) {
        Client *c = client_by_id(l->attach_id);
        if (c) return c->vw;
    } else if (l->attach_type == LABEL_ATTACH_VEHICLE) {
        Vehicle *v = vehicle_by_id(l->attach_id);
        if (v) return v->vw;
    }
    return l->vw;
}

/* ---------------- Broadcast paket label (vw-scoped ala SA-MP) ---------------- */

/* Isi frame 0x28 LABEL_SPAWN untuk satu label. Kembalikan panjang frame. */
static size_t label_spawn_frame(Label3D *l, uint8_t *b) {
    size_t tl = strlen(l->text);
    if (tl > LABEL_TEXT_MAX) tl = LABEL_TEXT_MAX;
    b[0] = P_LABEL_SPAWN;
    lput_u16(b + 1, (uint16_t)l->id);
    b[3] = (uint8_t)l->attach_type;
    lput_u16(b + 4, (uint16_t)(l->attach_id < 0 ? 0xFFFF : l->attach_id));
    lput_f32(b + 6, l->x);
    lput_f32(b + 10, l->y);
    lput_f32(b + 14, l->z);
    lput_f32(b + 18, l->drawdist);
    lput_u32(b + 22, l->color);
    b[26] = (uint8_t)tl;
    memcpy(b + 27, l->text, tl);
    return 27 + tl;
}

/* 0x28 LABEL_SPAWN ke semua viewer vw efektif label. */
static void bcast_label_spawn(Label3D *l) {
    uint8_t b[27 + LABEL_TEXT_MAX];
    broadcast_frame_vw(b, label_spawn_frame(l, b), NULL, label_eff_vw(l));
}

/* 0x29 LABEL_DESPAWN ke semua viewer vw tertentu. */
static void bcast_label_despawn(int id, uint32_t vw) {
    uint8_t b[3];
    b[0] = P_LABEL_DESPAWN;
    lput_u16(b + 1, (uint16_t)id);
    broadcast_frame_vw(b, 3, NULL, vw);
}

/* 0x2A LABEL_TEXT ke semua viewer vw efektif label. */
static void bcast_label_text(Label3D *l) {
    uint8_t b[7 + LABEL_TEXT_MAX];
    size_t tl = strlen(l->text);
    if (tl > LABEL_TEXT_MAX) tl = LABEL_TEXT_MAX;
    b[0] = P_LABEL_TEXT;
    lput_u16(b + 1, (uint16_t)l->id);
    lput_u32(b + 3, l->color);
    b[7] = (uint8_t)tl;
    memcpy(b + 8, l->text, tl);
    broadcast_frame_vw(b, 8 + tl, NULL, label_eff_vw(l));
}

/* 0x2B LABEL_POS ke semua viewer vw label (hanya label statis). */
static void bcast_label_pos(Label3D *l) {
    uint8_t b[15];
    b[0] = P_LABEL_POS;
    lput_u16(b + 1, (uint16_t)l->id);
    lput_f32(b + 3, l->x);
    lput_f32(b + 7, l->y);
    lput_f32(b + 11, l->z);
    broadcast_frame_vw(b, 15, NULL, l->vw);
}

/* Kirim 0x28 satu label ke satu client. */
static void send_label_to(Client *c, Label3D *l) {
    uint8_t b[27 + LABEL_TEXT_MAX];
    client_send_frame(c, b, label_spawn_frame(l, b));
}

/* ---------------- API inti ---------------- */

int label_create(const char *text, uint32_t color, float x, float y, float z,
                 float drawdist, uint32_t vw) {
    if (!text) text = "";
    int id = alloc_lid();
    if (id < 0) return -1;
    Label3D *l = (Label3D *)calloc(1, sizeof(Label3D));
    if (!l) return -1;
    l->id = id;
    strncpy(l->text, text, LABEL_TEXT_MAX);
    l->text[LABEL_TEXT_MAX] = '\0';
    l->color = color;
    l->x = x; l->y = y; l->z = z;
    l->drawdist = drawdist > 0 ? drawdist : LABEL_DRAW_DEFAULT;
    l->vw = vw;
    l->attach_type = LABEL_ATTACH_NONE;
    l->attach_id = -1;
    l->next = g_labels;
    g_labels = l;
    bcast_label_spawn(l);
    log_msg("[label] dibuat id %d (\"%.24s\") di %.1f, %.1f, %.1f vw %u",
            id, l->text, x, y, z, vw);
    return id;
}

int label_destroy(int id) {
    Label3D **pp = &g_labels;
    while (*pp) {
        if ((*pp)->id == id) {
            Label3D *l = *pp;
            bcast_label_despawn(id, label_eff_vw(l));
            *pp = l->next;
            log_msg("[label] dihancurkan id %d", id);
            free(l);
            return 1;
        }
        pp = &(*pp)->next;
    }
    return 0;
}

int label_set_text(int id, uint32_t color, const char *text) {
    Label3D *l = label_by_id(id);
    if (!l || !text) return 0;
    strncpy(l->text, text, LABEL_TEXT_MAX);
    l->text[LABEL_TEXT_MAX] = '\0';
    l->color = color;
    bcast_label_text(l);
    return 1;
}

int label_set_pos(int id, float x, float y, float z) {
    Label3D *l = label_by_id(id);
    if (!l) return 0;
    if (l->attach_type != LABEL_ATTACH_NONE) return 0; /* pakai attach ulang */
    l->x = x; l->y = y; l->z = z;
    bcast_label_pos(l);
    return 1;
}

/* Attach ulang: despawn dari vw lama + spawn ke vw entity baru. */
static int label_attach(int id, int type, int entity_id,
                        float ox, float oy, float oz) {
    Label3D *l = label_by_id(id);
    if (!l) return 0;
    if (type == LABEL_ATTACH_PLAYER && !client_by_id(entity_id)) return 0;
    if (type == LABEL_ATTACH_VEHICLE && !vehicle_by_id(entity_id)) return 0;
    uint32_t old_vw = label_eff_vw(l);
    bcast_label_despawn(id, old_vw);
    l->attach_type = type;
    l->attach_id = entity_id;
    l->x = ox; l->y = oy; l->z = oz;
    bcast_label_spawn(l);
    log_msg("[label] id %d di-attach ke %s %d (offset %.1f, %.1f, %.1f)",
            id, type == LABEL_ATTACH_PLAYER ? "pemain" : "kendaraan",
            entity_id, ox, oy, oz);
    return 1;
}

int label_attach_player(int id, int pid, float ox, float oy, float oz) {
    return label_attach(id, LABEL_ATTACH_PLAYER, pid, ox, oy, oz);
}

int label_attach_vehicle(int id, int vid, float ox, float oy, float oz) {
    return label_attach(id, LABEL_ATTACH_VEHICLE, vid, ox, oy, oz);
}

/* ---------------- Sync jaringan ---------------- */

/* Stream label ke pemain yang baru join: hanya yang vw-nya sama. */
void label_sync_join(Client *c) {
    for (Label3D *l = g_labels; l; l = l->next) {
        if (label_eff_vw(l) != c->vw) continue;
        send_label_to(c, l);
    }
}

/* Resync label saat pemain pindah vw. Label yang di-attach ke pemain itu
 * ikut pindah vw bersamanya -> despawn ke viewer vw lama + spawn ke
 * viewer vw baru. Label lain: despawn (ke c saja) yang vw lama, dan
 * yang vw barunya di-stream ke c di bawah. */
void label_sync_world(Client *c, uint32_t old_vw) {
    uint8_t d[3];
    d[0] = P_LABEL_DESPAWN;
    for (Label3D *l = g_labels; l; l = l->next) {
        if (l->attach_type == LABEL_ATTACH_PLAYER && l->attach_id == c->id) {
            /* label ikut pindah vw bersama pemain: resync semua viewer */
            if (old_vw == c->vw) continue;
            lput_u16(d + 1, (uint16_t)l->id);
            broadcast_frame_vw(d, 3, NULL, old_vw);
            bcast_label_spawn(l); /* vw baru = c->vw */
        } else if (label_eff_vw(l) == old_vw && old_vw != c->vw) {
            lput_u16(d + 1, (uint16_t)l->id);
            client_send_frame(c, d, 3);
        }
    }
    if (old_vw == c->vw) return;
    for (Label3D *l = g_labels; l; l = l->next) {
        if (l->attach_type == LABEL_ATTACH_PLAYER && l->attach_id == c->id)
            continue; /* sudah di-spawn ke semua viewer */
        if (label_eff_vw(l) == c->vw) send_label_to(c, l);
    }
}

/* Resync label yang di-attach ke kendaraan saat kendaraan pindah vw. */
void label_sync_vehicle_world(int vid, uint32_t old_vw) {
    Vehicle *v = vehicle_by_id(vid);
    if (!v) return;
    uint8_t d[3];
    d[0] = P_LABEL_DESPAWN;
    for (Label3D *l = g_labels; l; l = l->next) {
        if (l->attach_type != LABEL_ATTACH_VEHICLE || l->attach_id != vid)
            continue;
        if (old_vw == v->vw) continue;
        lput_u16(d + 1, (uint16_t)l->id);
        broadcast_frame_vw(d, 3, NULL, old_vw);
        bcast_label_spawn(l); /* vw baru = v->vw */
    }
}

/* Hapus semua label yang di-attach ke pemain yang disconnect.
 * Dipanggil selagi Client masih di list agar vw-nya terbaca. */
void label_on_player_leave(int pid) {
    int ids[LABEL_MAX];
    int n = 0;
    for (Label3D *l = g_labels; l && n < LABEL_MAX; l = l->next)
        if (l->attach_type == LABEL_ATTACH_PLAYER && l->attach_id == pid)
            ids[n++] = l->id;
    for (int i = 0; i < n; i++) label_destroy(ids[i]);
}

/* Hapus semua label yang di-attach ke kendaraan yang dihancurkan. */
void label_on_vehicle_destroy(int vid) {
    int ids[LABEL_MAX];
    int n = 0;
    for (Label3D *l = g_labels; l && n < LABEL_MAX; l = l->next)
        if (l->attach_type == LABEL_ATTACH_VEHICLE && l->attach_id == vid)
            ids[n++] = l->id;
    for (int i = 0; i < n; i++) label_destroy(ids[i]);
}
