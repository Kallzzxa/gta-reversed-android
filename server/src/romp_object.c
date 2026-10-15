/*
 * RO-MP object ala SA-MP — implementasi registry objek.
 * Lihat romp_object.h untuk desain protokol & API.
 */
#include "romp_object.h"
#include "romp_lua.h"   /* lua_on_object_moved */

#include <math.h>

static GameObject *g_objects = NULL;

/* ---- Helper frame (duplikat kecil dari romp_server.c agar modul mandiri) ---- */
static void oput_u16(uint8_t *b, uint16_t v) { b[0] = v & 0xFF; b[1] = (v >> 8) & 0xFF; }
static void oput_u32(uint8_t *b, uint32_t v) {
    oput_u16(b, (uint16_t)(v & 0xFFFF));
    oput_u16(b + 2, (uint16_t)(v >> 16));
}
static void oput_i32(uint8_t *b, int32_t v) {
    uint32_t u; memcpy(&u, &v, 4);
    oput_u32(b, u);
}
static void oput_f32(uint8_t *b, float v) {
    uint32_t u; memcpy(&u, &v, 4);
    oput_u16(b, (uint16_t)(u & 0xFFFF));
    oput_u16(b + 2, (uint16_t)(u >> 16));
}

/* ---------------- Registry ---------------- */

GameObject *object_by_id(int id) {
    if (id <= 0) return NULL;
    for (GameObject *o = g_objects; o; o = o->next)
        if (o->id == id) return o;
    return NULL;
}

GameObject *object_first(void) { return g_objects; }

int object_count(void) {
    int n = 0;
    for (GameObject *o = g_objects; o; o = o->next) n++;
    return n;
}

static int alloc_oid(void) {
    for (int id = 1; id <= OBJECT_MAX; id++)
        if (!object_by_id(id)) return id;
    return -1;
}

/* SA-MP: objek vw -1 terlihat di semua world, selain itu harus cocok. */
int object_visible_to(const GameObject *o, const Client *c) {
    if (!o || !c) return 0;
    if (o->vw == -1) return 1;
    return o->vw == (int32_t)c->vw;
}

/* ---------------- Broadcast paket objek ---------------- */

/* Isi frame 0x2C OBJ_SPAWN. Kembalikan panjang frame. */
static size_t object_spawn_frame(GameObject *o, uint8_t *b) {
    b[0] = P_OBJECT_SPAWN;
    oput_u16(b + 1, (uint16_t)o->id);
    oput_u32(b + 3, o->modelid);
    oput_f32(b + 7, o->x);
    oput_f32(b + 11, o->y);
    oput_f32(b + 15, o->z);
    oput_f32(b + 19, o->rx);
    oput_f32(b + 23, o->ry);
    oput_f32(b + 27, o->rz);
    oput_f32(b + 31, o->drawdist);
    oput_i32(b + 35, o->vw);
    return 39;
}

/* 0x2C ke semua viewer objek. */
static void bcast_object_spawn(GameObject *o) {
    uint8_t b[39];
    size_t n = object_spawn_frame(o, b);
    for (Client *c = g_clients; c; c = c->next)
        if (object_visible_to(o, c)) client_send_frame(c, b, n);
}

/* 0x2D ke viewer yang diberi (NULL viewer = semua yang melihat objek). */
static void bcast_object_despawn_viewers(int id, const GameObject *o) {
    uint8_t b[3];
    b[0] = P_OBJECT_DESPAWN;
    oput_u16(b + 1, (uint16_t)id);
    for (Client *c = g_clients; c; c = c->next)
        if (object_visible_to(o, c)) client_send_frame(c, b, 3);
}

/* Isi frame 0x2E OBJ_MOVE untuk gerakan yang sedang berjalan. */
static size_t object_move_frame(GameObject *o, uint8_t *b) {
    b[0] = P_OBJECT_MOVE;
    oput_u16(b + 1, (uint16_t)o->id);
    oput_f32(b + 3, o->tx);
    oput_f32(b + 7, o->ty);
    oput_f32(b + 11, o->tz);
    oput_f32(b + 15, o->speed);
    oput_f32(b + 19, o->trx);
    oput_f32(b + 23, o->try_);
    oput_f32(b + 27, o->trz);
    return 31;
}

/* 0x2E ke semua viewer objek. */
static void bcast_object_move(GameObject *o) {
    uint8_t b[31];
    size_t n = object_move_frame(o, b);
    for (Client *c = g_clients; c; c = c->next)
        if (object_visible_to(o, c)) client_send_frame(c, b, n);
}

/* Isi frame 0x2F OBJ_UPDATE (posisi+rotasi instan). */
static size_t object_update_frame(GameObject *o, uint8_t *b) {
    b[0] = P_OBJECT_UPDATE;
    oput_u16(b + 1, (uint16_t)o->id);
    oput_f32(b + 3, o->x);
    oput_f32(b + 7, o->y);
    oput_f32(b + 11, o->z);
    oput_f32(b + 15, o->rx);
    oput_f32(b + 19, o->ry);
    oput_f32(b + 23, o->rz);
    return 27;
}

/* 0x2F ke semua viewer objek. */
static void bcast_object_update(GameObject *o) {
    uint8_t b[27];
    size_t n = object_update_frame(o, b);
    for (Client *c = g_clients; c; c = c->next)
        if (object_visible_to(o, c)) client_send_frame(c, b, n);
}

/* ---------------- API inti ---------------- */

int object_create(uint32_t modelid, float x, float y, float z,
                  float rx, float ry, float rz, float drawdist) {
    if (modelid == 0) return -1;
    int id = alloc_oid();
    if (id < 0) return -1;
    GameObject *o = (GameObject *)calloc(1, sizeof(GameObject));
    if (!o) return -1;
    o->id = id;
    o->modelid = modelid;
    o->x = x; o->y = y; o->z = z;
    o->rx = rx; o->ry = ry; o->rz = rz;
    o->drawdist = drawdist > 0 ? drawdist : OBJECT_DRAW_DEFAULT;
    o->vw = 0;
    o->next = g_objects;
    g_objects = o;
    bcast_object_spawn(o);
    log_msg("[object] dibuat id %d (model %u) di %.1f, %.1f, %.1f",
            id, modelid, x, y, z);
    return id;
}

int object_destroy(int id) {
    GameObject **pp = &g_objects;
    while (*pp) {
        if ((*pp)->id == id) {
            GameObject *o = *pp;
            bcast_object_despawn_viewers(id, o);
            *pp = o->next;
            log_msg("[object] dihancurkan id %d", id);
            free(o);
            return 1;
        }
        pp = &(*pp)->next;
    }
    return 0;
}

/* Jarak Euclidean posisi saat ini ke target. */
static float move_remaining(GameObject *o) {
    float dx = o->tx - o->x, dy = o->ty - o->y, dz = o->tz - o->z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

int object_move(int id, float tx, float ty, float tz, float speed,
                float trx, float try_, float trz) {
    GameObject *o = object_by_id(id);
    if (!o) return 0;
    if (!(speed > 0)) return 0;          /* speed harus positif */
    o->sx = o->x; o->sy = o->y; o->sz = o->z;
    o->tx = tx; o->ty = ty; o->tz = tz;
    o->srx = o->rx; o->sry = o->ry; o->srz = o->rz;
    o->trx = trx; o->try_ = try_; o->trz = trz;
    o->speed = speed;
    o->move_dist = move_remaining(o);
    if (o->move_dist < 0.0001f) {
        /* Sudah di target: tak perlu bergerak; rotasi tetap diterapkan. */
        if (trx != OBJECT_NO_ROT) o->rx = trx;
        if (try_ != OBJECT_NO_ROT) o->ry = try_;
        if (trz != OBJECT_NO_ROT) o->rz = trz;
        o->moving = 0;
        bcast_object_update(o);
        lua_on_object_moved(id);
        return 1;
    }
    o->moving = 1;
    o->move_start_ms = now_ms();
    bcast_object_move(o);
    log_msg("[object] id %d bergerak ke %.1f, %.1f, %.1f (speed %.1f)",
            id, tx, ty, tz, speed);
    return 1;
}

int object_stop(int id) {
    GameObject *o = object_by_id(id);
    if (!o || !o->moving) return 0;
    /* Bekukan di posisi saat ini (dimajukan dulu ke waktu sekarang). */
    object_tick(now_ms());
    if (o->moving) {
        o->moving = 0;
        bcast_object_update(o);
    }
    return 1;
}

int object_is_moving(int id) {
    GameObject *o = object_by_id(id);
    return o ? o->moving : 0;
}

int object_set_pos(int id, float x, float y, float z) {
    GameObject *o = object_by_id(id);
    if (!o) return 0;
    o->moving = 0;
    o->x = x; o->y = y; o->z = z;
    bcast_object_update(o);
    return 1;
}

int object_set_rot(int id, float rx, float ry, float rz) {
    GameObject *o = object_by_id(id);
    if (!o) return 0;
    o->rx = rx; o->ry = ry; o->rz = rz;
    bcast_object_update(o);
    return 1;
}

int object_set_vw(int id, int32_t vw) {
    GameObject *o = object_by_id(id);
    if (!o) return 0;
    if (o->vw == vw) return 1;
    /* Despawn ke viewer lama. */
    uint8_t d[3];
    d[0] = P_OBJECT_DESPAWN;
    oput_u16(d + 1, (uint16_t)id);
    for (Client *c = g_clients; c; c = c->next)
        if (object_visible_to(o, c)) client_send_frame(c, d, 3);
    o->vw = vw;
    /* Spawn ke viewer baru (bila bergerak, sertakan 0x2E sisanya). */
    uint8_t b[39];
    size_t n = object_spawn_frame(o, b);
    uint8_t m[31];
    size_t mn = o->moving ? object_move_frame(o, m) : 0;
    for (Client *c = g_clients; c; c = c->next) {
        if (!object_visible_to(o, c)) continue;
        client_send_frame(c, b, n);
        if (mn) client_send_frame(c, m, mn);
    }
    log_msg("[object] id %d pindah vw %d", id, (int)vw);
    return 1;
}

/* ---------------- Tick gerakan ---------------- */

void object_tick(uint64_t tnow_ms) {
    for (GameObject *o = g_objects; o; o = o->next) {
        if (!o->moving) continue;
        float elapsed = (tnow_ms - o->move_start_ms) / 1000.0f;
        float traveled = o->speed * elapsed;
        if (o->move_dist <= 0) continue; /* pengaman */
        float frac = traveled / o->move_dist;
        if (frac >= 1.0f) {
            /* Tiba: kunci ke target persis. */
            o->x = o->tx; o->y = o->ty; o->z = o->tz;
            if (o->trx != OBJECT_NO_ROT) o->rx = o->trx;
            if (o->try_ != OBJECT_NO_ROT) o->ry = o->try_;
            if (o->trz != OBJECT_NO_ROT) o->rz = o->trz;
            o->moving = 0;
            bcast_object_update(o); /* posisi final persis ke semua viewer */
            log_msg("[object] id %d tiba di target", o->id);
            lua_on_object_moved(o->id);
        } else {
            float dx = o->tx - o->sx, dy = o->ty - o->sy, dz = o->tz - o->sz;
            o->x = o->sx + dx * frac;
            o->y = o->sy + dy * frac;
            o->z = o->sz + dz * frac;
            if (o->trx != OBJECT_NO_ROT) o->rx = o->srx + (o->trx - o->srx) * frac;
            if (o->try_ != OBJECT_NO_ROT) o->ry = o->sry + (o->try_ - o->sry) * frac;
            if (o->trz != OBJECT_NO_ROT) o->rz = o->srz + (o->trz - o->srz) * frac;
        }
    }
}

/* ---------------- Sync jaringan ---------------- */

/* Stream objek ke pemain yang baru join. */
void object_sync_join(Client *c) {
    uint8_t b[39], m[31];
    for (GameObject *o = g_objects; o; o = o->next) {
        if (!object_visible_to(o, c)) continue;
        client_send_frame(c, b, object_spawn_frame(o, b));
        if (o->moving)
            client_send_frame(c, m, object_move_frame(o, m));
    }
}

/* Resync objek saat pemain pindah vw. */
void object_sync_world(Client *c, uint32_t old_vw) {
    uint8_t d[3];
    d[0] = P_OBJECT_DESPAWN;
    uint8_t b[39], m[31];
    for (GameObject *o = g_objects; o; o = o->next) {
        int was = (o->vw == -1 || o->vw == (int32_t)old_vw);
        int now = object_visible_to(o, c);
        if (was && !now) {
            oput_u16(d + 1, (uint16_t)o->id);
            client_send_frame(c, d, 3);
        } else if (!was && now) {
            client_send_frame(c, b, object_spawn_frame(o, b));
            if (o->moving)
                client_send_frame(c, m, object_move_frame(o, m));
        }
    }
}
