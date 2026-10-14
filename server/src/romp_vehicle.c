/*
 * RO-MP vehicle sync ala SA-MP — implementasi registry kendaraan.
 * Lihat romp_vehicle.h untuk desain protokol & API.
 */
#include "romp_vehicle.h"
#include "romp_lua.h"

static Vehicle *g_vehicles = NULL;

/* ---- Helper frame (duplikat kecil dari romp_server.c agar modul mandiri) ---- */
static void vput_u16(uint8_t *b, uint16_t v) { b[0] = v & 0xFF; b[1] = (v >> 8) & 0xFF; }
static void vput_f32(uint8_t *b, float v) { uint32_t u; memcpy(&u, &v, 4); vput_u16(b, (uint16_t)(u & 0xFFFF)); vput_u16(b + 2, (uint16_t)(u >> 16)); }
static uint16_t vget_u16(const uint8_t *b) { return (uint16_t)(b[0] | (b[1] << 8)); }
static float vget_f32(const uint8_t *b) {
    uint32_t u = (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
                 ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    float f; memcpy(&f, &u, 4); return f;
}

/* ---------------- Registry ---------------- */

Vehicle *vehicle_by_id(int id) {
    if (id <= 0) return NULL;
    for (Vehicle *v = g_vehicles; v; v = v->next)
        if (v->id == id) return v;
    return NULL;
}

int vehicle_count(void) {
    int n = 0;
    for (Vehicle *v = g_vehicles; v; v = v->next) n++;
    return n;
}

Vehicle *vehicle_first(void) { return g_vehicles; }
Vehicle *vehicle_next(Vehicle *v) { return v ? v->next : NULL; }

static int alloc_vid(void) {
    for (int id = 1; id <= VEH_MAX; id++)
        if (!vehicle_by_id(id)) return id;
    return -1;
}

/* ---------------- Broadcast paket kendaraan ---------------- */

/* 0x13 VEH_SYNC: u16 driverid | u16 vid | f32 x,y,z | f32 heading | u16 health.
 * driverid 0xFFFF = update server-side (teleport/respawn), bukan dari sopir. */
static void bcast_veh_sync(Vehicle *v, int driverid) {
    uint8_t b[24];
    b[0] = P_VEH_SYNC;
    vput_u16(b + 1, (uint16_t)(driverid < 0 ? 0xFFFF : driverid));
    vput_u16(b + 3, (uint16_t)v->id);
    vput_f32(b + 5, v->x);
    vput_f32(b + 9, v->y);
    vput_f32(b + 13, v->z);
    vput_f32(b + 17, v->heading);
    vput_u16(b + 21, v->health);
    broadcast_frame(b, 23, NULL);
}

/* 0x14 VEH_SPAWN: u16 vid | u16 model | f32 x,y,z | f32 angle |
 *                 u8 c1 | u8 c2 | u16 health | u16 driverid (0xFFFF = kosong) */
static void bcast_veh_spawn(Vehicle *v) {
    uint8_t b[27];
    b[0] = P_VEH_SPAWN;
    vput_u16(b + 1, (uint16_t)v->id);
    vput_u16(b + 3, v->model);
    vput_f32(b + 5, v->x);
    vput_f32(b + 9, v->y);
    vput_f32(b + 13, v->z);
    vput_f32(b + 17, v->sangle);
    b[21] = v->c1;
    b[22] = v->c2;
    vput_u16(b + 23, v->health);
    vput_u16(b + 25, (uint16_t)(v->driver < 0 ? 0xFFFF : v->driver));
    broadcast_frame(b, 27, NULL);
}

/* 0x15 VEH_DESPAWN: u16 vid */
static void bcast_veh_despawn(int vid) {
    uint8_t b[3];
    b[0] = P_VEH_DESPAWN;
    vput_u16(b + 1, (uint16_t)vid);
    broadcast_frame(b, 3, NULL);
}

/* 0x16 VEH_SEAT: u16 vid | u8 seat | u16 pid (0xFFFF = kosong) */
static void bcast_veh_seat(int vid, int seat, int pid) {
    uint8_t b[6];
    b[0] = P_VEH_SEAT;
    vput_u16(b + 1, (uint16_t)vid);
    b[3] = (uint8_t)seat;
    vput_u16(b + 4, (uint16_t)(pid < 0 ? 0xFFFF : pid));
    broadcast_frame(b, 6, NULL);
}

/* ---------------- API inti ---------------- */

int vehicle_create(uint16_t model, float x, float y, float z, float angle,
                   uint8_t c1, uint8_t c2, uint32_t respawn_ms) {
    /* id model kendaraan SA-MP: 400..611 */
    if (model < 400 || model > 611) return -1;
    int id = alloc_vid();
    if (id < 0) return -1;
    Vehicle *v = (Vehicle *)calloc(1, sizeof(Vehicle));
    if (!v) return -1;
    v->id = id;
    v->model = model;
    v->sx = v->x = x; v->sy = v->y = y; v->sz = v->z = z;
    v->sangle = v->heading = angle;
    v->c1 = c1; v->c2 = c2;
    v->health = VEH_HEALTH_MAX;
    v->driver = -1;
    v->pass[0] = v->pass[1] = v->pass[2] = -1;
    v->respawn_ms = respawn_ms;
    v->next = g_vehicles;
    g_vehicles = v;
    bcast_veh_spawn(v);
    log_msg("[veh] dibuat id %d (model %u) di %.1f, %.1f, %.1f", id, model, x, y, z);
    return id;
}

/* Keluarkan semua penumpang; fire_exit=1 -> event Lua, 0 -> diam. */
static void eject_all(Vehicle *v, int fire_exit) {
    int pids[VEH_SEATS];
    pids[0] = v->driver;
    pids[1] = v->pass[0]; pids[2] = v->pass[1]; pids[3] = v->pass[2];
    v->driver = -1;
    v->pass[0] = v->pass[1] = v->pass[2] = -1;
    for (int s = 0; s < VEH_SEATS; s++) {
        if (pids[s] < 0) continue;
        Client *c = client_by_id(pids[s]);
        if (c) { c->vehicle_id = -1; c->vehicle_seat = -1; }
        bcast_veh_seat(v->id, s, -1);
        if (fire_exit) lua_on_player_exit_vehicle(pids[s], v->id);
    }
}

int vehicle_destroy(int id) {
    Vehicle **pp = &g_vehicles;
    while (*pp) {
        if ((*pp)->id == id) {
            Vehicle *v = *pp;
            eject_all(v, 1);
            bcast_veh_despawn(id);
            *pp = v->next;
            free(v);
            log_msg("[veh] dihancurkan id %d", id);
            return 1;
        }
        pp = &(*pp)->next;
    }
    return 0;
}

/* Urutan mati: tandai destroyed, keluarkan penumpang, broadcast, event. */
static void vehicle_kill(Vehicle *v, int killerid) {
    if (v->destroyed) return;
    v->destroyed = 1;
    v->dead_at = now_ms();
    v->health = 0;
    eject_all(v, 1);
    bcast_veh_sync(v, -1);
    log_msg("[veh] id %d hancur", v->id);
    lua_on_vehicle_death(v->id, killerid);
}

int vehicle_set_pos(int id, float x, float y, float z, float angle) {
    Vehicle *v = vehicle_by_id(id);
    if (!v || v->destroyed) return 0;
    v->x = x; v->y = y; v->z = z; v->heading = angle;
    bcast_veh_sync(v, -1); /* driverless = otoritatif dari server */
    return 1;
}

int vehicle_set_health(int id, uint16_t health, int killerid) {
    Vehicle *v = vehicle_by_id(id);
    if (!v) return 0;
    if (health > VEH_HEALTH_MAX) health = VEH_HEALTH_MAX;
    if (v->destroyed) {
        /* Perbaiki kendaraan yang hancur = hidup lagi di tempat. */
        if (health <= VEH_HEALTH_WRECK) return 1;
        v->destroyed = 0;
        v->health = health;
        bcast_veh_sync(v, -1);
        lua_on_vehicle_spawn(v->id);
        return 1;
    }
    if (health == v->health) return 1;
    if (health < v->health)
        lua_on_vehicle_damage(v->id, killerid, health);
    v->health = health;
    if (health <= VEH_HEALTH_WRECK) {
        vehicle_kill(v, killerid);
        return 1;
    }
    bcast_veh_sync(v, v->driver);
    return 1;
}

int vehicle_damage(int id, uint16_t amount, int killerid) {
    Vehicle *v = vehicle_by_id(id);
    if (!v || v->destroyed) return -1;
    uint16_t nh = v->health > amount ? (uint16_t)(v->health - amount) : 0;
    vehicle_set_health(id, nh, killerid);
    return (int)v->health;
}

int vehicle_respawn(int id) {
    Vehicle *v = vehicle_by_id(id);
    if (!v) return 0;
    v->destroyed = 0;
    v->health = VEH_HEALTH_MAX;
    v->x = v->sx; v->y = v->sy; v->z = v->sz; v->heading = v->sangle;
    bcast_veh_sync(v, -1);
    lua_on_vehicle_spawn(v->id);
    log_msg("[veh] id %d di-respawn", id);
    return 1;
}

int vehicle_set_colors(int id, uint8_t c1, uint8_t c2) {
    Vehicle *v = vehicle_by_id(id);
    if (!v) return 0;
    v->c1 = c1; v->c2 = c2;
    bcast_veh_spawn(v); /* spawn ulang di client = warna baru */
    return 1;
}

/* ---------------- Penumpang ---------------- */

int vehicle_put_player(int pid, int vid, int seat) {
    Client *c = client_by_id(pid);
    Vehicle *v = vehicle_by_id(vid);
    if (!c || !v || v->destroyed) return 0;
    if (seat < 0 || seat >= VEH_SEATS) return 0;
    if (c->vehicle_id == vid && c->vehicle_seat == seat) return 1;
    int taken = (seat == 0) ? v->driver : v->pass[seat - 1];
    if (taken >= 0) return 0; /* kursi sudah ada yang duduki */
    if (c->vehicle_id >= 0)
        vehicle_remove_player(pid); /* pindah kendaraan: keluar dulu */
    if (seat == 0) v->driver = pid;
    else v->pass[seat - 1] = pid;
    c->vehicle_id = vid;
    c->vehicle_seat = seat;
    bcast_veh_seat(vid, seat, pid);
    lua_on_player_enter_vehicle(pid, vid, seat);
    return 1;
}

static int remove_player_impl(int pid, int fire_exit) {
    Client *c = client_by_id(pid);
    if (!c || c->vehicle_id < 0) return 0;
    Vehicle *v = vehicle_by_id(c->vehicle_id);
    int vid = c->vehicle_id;
    int seat = c->vehicle_seat;
    c->vehicle_id = -1;
    c->vehicle_seat = -1;
    if (v) {
        if (seat == 0 && v->driver == pid) v->driver = -1;
        else if (seat >= 1 && seat < VEH_SEATS && v->pass[seat - 1] == pid)
            v->pass[seat - 1] = -1;
        bcast_veh_seat(vid, seat, -1);
    }
    if (fire_exit) lua_on_player_exit_vehicle(pid, vid);
    return 1;
}

int vehicle_remove_player(int pid) {
    return remove_player_impl(pid, 1);
}

void vehicle_remove_player_silent(int pid) {
    remove_player_impl(pid, 0);
}

int vehicle_of_player(int pid) {
    Client *c = client_by_id(pid);
    if (!c) return -1;
    return c->vehicle_id;
}

int vehicle_seat_of_player(int pid) {
    Client *c = client_by_id(pid);
    if (!c || c->vehicle_id < 0) return -1;
    return c->vehicle_seat;
}

int vehicle_occupant(int vid, int seat) {
    Vehicle *v = vehicle_by_id(vid);
    if (!v || seat < 0 || seat >= VEH_SEATS) return -1;
    return (seat == 0) ? v->driver : v->pass[seat - 1];
}

/* ---------------- Sync jaringan ---------------- */

/* Paket 0x12 VEH dari client: u16 vid | f32 x,y,z | f32 heading | u16 health.
 * Hanya sopir yang boleh sync; selain itu diabaikan (anti-cheat dasar). */
void vehicle_on_packet(Client *c, const uint8_t *p, size_t n) {
    if (!c->handshaked || n < 20) return;
    int vid = (int)vget_u16(p);
    Vehicle *v = vehicle_by_id(vid);
    if (!v || v->destroyed) return;
    if (v->driver != c->id || c->vehicle_id != vid || c->vehicle_seat != 0)
        return; /* bukan sopir: abaikan */
    uint64_t t = now_ms();
    if (t - v->last_sync_ms < 30) return; /* rate limit ~33 Hz */
    v->last_sync_ms = t;
    v->x = vget_f32(p + 2);
    v->y = vget_f32(p + 6);
    v->z = vget_f32(p + 10);
    v->heading = vget_f32(p + 14);
    uint16_t health = vget_u16(p + 18);
    if (health > VEH_HEALTH_MAX) health = VEH_HEALTH_MAX;
    if (health != v->health) {
        /* Damage/heal yang dilaporkan sopir lewat sync. */
        vehicle_set_health(vid, health, -1);
        return; /* vehicle_set_health sudah broadcast */
    }
    bcast_veh_sync(v, c->id);
}

void vehicle_sync_join(Client *c) {
    for (Vehicle *v = g_vehicles; v; v = v->next) {
        if (v->destroyed) continue;
        uint8_t b[27];
        b[0] = P_VEH_SPAWN;
        vput_u16(b + 1, (uint16_t)v->id);
        vput_u16(b + 3, v->model);
        vput_f32(b + 5, v->x);
        vput_f32(b + 9, v->y);
        vput_f32(b + 13, v->z);
        vput_f32(b + 17, v->sangle);
        b[21] = v->c1;
        b[22] = v->c2;
        vput_u16(b + 23, v->health);
        vput_u16(b + 25, (uint16_t)(v->driver < 0 ? 0xFFFF : v->driver));
        client_send_frame(c, b, 27);
        for (int s = 0; s < VEH_SEATS; s++) {
            int occ = vehicle_occupant(v->id, s);
            if (occ < 0) continue;
            uint8_t sb[6];
            sb[0] = P_VEH_SEAT;
            vput_u16(sb + 1, (uint16_t)v->id);
            sb[3] = (uint8_t)s;
            vput_u16(sb + 4, (uint16_t)occ);
            client_send_frame(c, sb, 6);
        }
    }
}

void vehicle_tick(uint64_t now) {
    for (Vehicle *v = g_vehicles; v; v = v->next) {
        if (v->destroyed && v->respawn_ms > 0 &&
            now - v->dead_at >= v->respawn_ms) {
            v->destroyed = 0;
            v->health = VEH_HEALTH_MAX;
            v->x = v->sx; v->y = v->sy; v->z = v->sz; v->heading = v->sangle;
            bcast_veh_sync(v, -1);
            log_msg("[veh] id %d respawn otomatis", v->id);
            lua_on_vehicle_spawn(v->id);
        }
    }
}
