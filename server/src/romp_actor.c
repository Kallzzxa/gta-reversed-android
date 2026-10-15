/*
 * RO-MP actor statis ala SA-MP — implementasi registry actor.
 * Lihat romp_actor.h untuk desain protokol & API.
 */
#include "romp_actor.h"
#include "romp_lua.h"  /* event onPlayerGiveDamageActor / onActorDeath */

static Actor *g_actors = NULL;

/* ---- Helper frame (duplikat kecil dari romp_server.c agar modul mandiri) ---- */
static void aput_u16(uint8_t *b, uint16_t v) { b[0] = v & 0xFF; b[1] = (v >> 8) & 0xFF; }
static void aput_u32(uint8_t *b, uint32_t v) {
    aput_u16(b, (uint16_t)(v & 0xFFFF));
    aput_u16(b + 2, (uint16_t)(v >> 16));
}
static void aput_f32(uint8_t *b, float v) {
    uint32_t u; memcpy(&u, &v, 4);
    aput_u16(b, (uint16_t)(u & 0xFFFF));
    aput_u16(b + 2, (uint16_t)(u >> 16));
}

/* ---- Damage ala GTA:SA per weaponid (dipakai actor_damage).
 * Client melaporkan tembakan, server menghitung damage sendiri. */
static float actor_weapon_damage(int wid) {
    switch (wid) {
        case 1:  return 4.0f;   /* brass knuckles */
        case 2:  return 5.0f;   /* golf club */
        case 3:  return 5.0f;   /* nightstick */
        case 4:  return 8.0f;   /* knife */
        case 5:  return 10.0f;  /* bat */
        case 6:  return 10.0f;  /* shovel */
        case 7:  return 8.0f;   /* pool cue */
        case 8:  return 10.0f;  /* katana */
        case 9:  return 25.0f;  /* chainsaw */
        case 10: return 8.0f;   /* dildo */
        case 12: return 8.0f;   /* cane */
        case 16: return 82.0f;  /* grenade */
        case 18: return 82.0f;  /* molotov */
        case 22: return 25.0f;  /* colt45 */
        case 23: return 40.0f;  /* silenced */
        case 24: return 46.0f;  /* deagle */
        case 25: return 49.0f;  /* shotgun */
        case 26: return 49.0f;  /* sawnoff */
        case 27: return 39.0f;  /* spas12 */
        case 28: return 20.0f;  /* uzi */
        case 29: return 25.0f;  /* mp5 */
        case 30: return 30.0f;  /* ak47 */
        case 31: return 30.0f;  /* m4 */
        case 32: return 20.0f;  /* tec9 */
        case 33: return 75.0f;  /* rifle */
        case 34: return 41.0f;  /* sniper */
        case 35: return 82.0f;  /* rocket */
        case 36: return 82.0f;  /* hs rocket */
        case 37: return 2.0f;   /* flamethrower (per shot) */
        case 38: return 46.0f;  /* minigun */
        case 39: return 82.0f;  /* satchel */
        default: return 2.5f;   /* fist / senjata lain */
    }
}

/* ---------------- Registry ---------------- */

Actor *actor_by_id(int id) {
    if (id <= 0) return NULL;
    for (Actor *a = g_actors; a; a = a->next)
        if (a->id == id) return a;
    return NULL;
}

int actor_is_valid(int id) { return actor_by_id(id) != NULL; }

Actor *actor_first(void) { return g_actors; }

int actor_count(void) {
    int n = 0;
    for (Actor *a = g_actors; a; a = a->next) n++;
    return n;
}

static int alloc_aid(void) {
    for (int id = 1; id <= ACTOR_MAX; id++)
        if (!actor_by_id(id)) return id;
    return -1;
}

/* ---------------- Broadcast paket actor (vw-scoped ala SA-MP) ---------------- */

/* Isi frame 0x32 ACTOR_SPAWN untuk satu actor. Kembalikan panjang frame. */
static size_t actor_spawn_frame(Actor *a, uint8_t *b) {
    b[0] = P_ACTOR_SPAWN;
    aput_u16(b + 1, (uint16_t)a->id);
    aput_u16(b + 3, (uint16_t)a->modelid);
    aput_f32(b + 5, a->x);
    aput_f32(b + 9, a->y);
    aput_f32(b + 13, a->z);
    aput_f32(b + 17, a->angle);
    aput_u16(b + 21, (uint16_t)a->health);
    b[23] = (uint8_t)(a->invulnerable ? 1 : 0);
    aput_u32(b + 24, a->vw);
    return 28;
}

/* 0x32 ACTOR_SPAWN ke semua viewer vw actor. */
static void bcast_actor_spawn(Actor *a) {
    uint8_t b[28];
    broadcast_frame_vw(b, actor_spawn_frame(a, b), NULL, a->vw);
}

/* 0x33 ACTOR_DESPAWN ke semua viewer vw tertentu. */
static void bcast_actor_despawn(int id, uint32_t vw) {
    uint8_t b[3];
    b[0] = P_ACTOR_DESPAWN;
    aput_u16(b + 1, (uint16_t)id);
    broadcast_frame_vw(b, 3, NULL, vw);
}

/* 0x34 ACTOR_POS ke semua viewer vw actor. */
static void bcast_actor_pos(Actor *a) {
    uint8_t b[19];
    b[0] = P_ACTOR_POS;
    aput_u16(b + 1, (uint16_t)a->id);
    aput_f32(b + 3, a->x);
    aput_f32(b + 7, a->y);
    aput_f32(b + 11, a->z);
    aput_f32(b + 15, a->angle);
    broadcast_frame_vw(b, 19, NULL, a->vw);
}

/* 0x35 ACTOR_ANIM ke semua viewer vw actor. */
static void bcast_actor_anim(Actor *a) {
    uint8_t b[3 + ACTOR_ANIM_MAX];
    size_t al = strlen(a->anim);
    if (al > ACTOR_ANIM_MAX) al = ACTOR_ANIM_MAX;
    b[0] = P_ACTOR_ANIM;
    aput_u16(b + 1, (uint16_t)a->id);
    b[3] = (uint8_t)al;
    memcpy(b + 4, a->anim, al);
    broadcast_frame_vw(b, 4 + al, NULL, a->vw);
}

/* 0x36 ACTOR_HEALTH ke semua viewer vw actor. */
static void bcast_actor_health(Actor *a) {
    uint8_t b[5];
    b[0] = P_ACTOR_HEALTH;
    aput_u16(b + 1, (uint16_t)a->id);
    aput_u16(b + 3, (uint16_t)a->health);
    broadcast_frame_vw(b, 5, NULL, a->vw);
}

/* Kirim 0x32 satu actor ke satu client. */
static void send_actor_to(Client *c, Actor *a) {
    uint8_t b[28];
    client_send_frame(c, b, actor_spawn_frame(a, b));
}

/* ---------------- API inti ---------------- */

int actor_create(int modelid, float x, float y, float z, float angle) {
    if (modelid < 0 || modelid > ACTOR_MODEL_MAX) return -1;
    int id = alloc_aid();
    if (id < 0) return -1;
    Actor *a = (Actor *)calloc(1, sizeof(Actor));
    if (!a) return -1;
    a->id = id;
    a->modelid = modelid;
    a->x = x; a->y = y; a->z = z;
    a->angle = angle;
    a->health = ACTOR_HEALTH_MAX;
    a->invulnerable = 0;
    a->vw = 0;
    a->anim[0] = '\0';
    a->next = g_actors;
    g_actors = a;
    bcast_actor_spawn(a);
    log_msg("[actor] dibuat id %d model %d di %.1f, %.1f, %.1f", id, modelid, x, y, z);
    return id;
}

int actor_destroy(int id) {
    Actor **pp = &g_actors;
    while (*pp) {
        if ((*pp)->id == id) {
            Actor *a = *pp;
            bcast_actor_despawn(id, a->vw);
            *pp = a->next;
            log_msg("[actor] dihancurkan id %d", id);
            free(a);
            return 1;
        }
        pp = &(*pp)->next;
    }
    return 0;
}

int actor_set_pos(int id, float x, float y, float z, float angle) {
    Actor *a = actor_by_id(id);
    if (!a) return 0;
    a->x = x; a->y = y; a->z = z; a->angle = angle;
    bcast_actor_pos(a);
    return 1;
}

int actor_set_health(int id, int health) {
    Actor *a = actor_by_id(id);
    if (!a) return 0;
    if (health < 0) health = 0;
    if (health > ACTOR_HEALTH_MAX) health = ACTOR_HEALTH_MAX;
    if (a->health == health) return 1;
    a->health = health;
    bcast_actor_health(a);
    return 1;
}

int actor_set_invulnerable(int id, int invuln) {
    Actor *a = actor_by_id(id);
    if (!a) return 0;
    a->invulnerable = invuln ? 1 : 0;
    bcast_actor_spawn(a); /* flag berubah: client spawn ulang */
    return 1;
}

int actor_apply_anim(int id, const char *anim) {
    Actor *a = actor_by_id(id);
    if (!a) return 0;
    if (!anim) anim = "";
    strncpy(a->anim, anim, ACTOR_ANIM_MAX);
    a->anim[ACTOR_ANIM_MAX] = '\0';
    bcast_actor_anim(a);
    return 1;
}

int actor_clear_anim(int id) {
    return actor_apply_anim(id, "");
}

int actor_set_vw(int id, uint32_t vw) {
    Actor *a = actor_by_id(id);
    if (!a) return 0;
    uint32_t old = a->vw;
    if (old == vw) return 1;
    bcast_actor_despawn(id, old);
    a->vw = vw;
    bcast_actor_spawn(a);
    log_msg("[actor] id %d pindah vw %u -> %u", id, old, vw);
    return 1;
}

/* Damage actor oleh pemain. Dipicu validasi WPN_SHOT hittype 4. */
int actor_damage(int pid, int actorid, int weaponid) {
    Actor *a = actor_by_id(actorid);
    if (!a) return 0;
    float amount = actor_weapon_damage(weaponid);
    lua_on_player_give_damage_actor(pid, actorid, amount, weaponid);
    if (!a->invulnerable && a->health > 0) {
        int newh = a->health - (int)amount;
        if (newh <= 0) {
            a->health = 0;
            bcast_actor_health(a);
            log_msg("[actor] id %d dibunuh oleh pemain %d (weapon %d)",
                    actorid, pid, weaponid);
            lua_on_actor_death(actorid, pid);
        } else {
            a->health = newh;
            bcast_actor_health(a);
        }
    }
    return 1;
}

/* ---------------- Sync jaringan ---------------- */

/* Stream actor ke pemain yang baru join: hanya yang vw-nya sama. */
void actor_sync_join(Client *c) {
    for (Actor *a = g_actors; a; a = a->next) {
        if (a->vw != c->vw) continue;
        send_actor_to(c, a);
    }
}

/* Resync actor saat pemain pindah vw. */
void actor_sync_world(Client *c, uint32_t old_vw) {
    uint8_t d[3];
    d[0] = P_ACTOR_DESPAWN;
    for (Actor *a = g_actors; a; a = a->next) {
        if (a->vw == old_vw && old_vw != c->vw) {
            aput_u16(d + 1, (uint16_t)a->id);
            client_send_frame(c, d, 3);
        }
    }
    if (old_vw == c->vw) return;
    for (Actor *a = g_actors; a; a = a->next) {
        if (a->vw == c->vw) send_actor_to(c, a);
    }
}
