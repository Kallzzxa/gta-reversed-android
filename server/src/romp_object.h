/*
 * RO-MP object ala SA-MP (CreateObject/DestroyObject/MoveObject/SetObjectPos/
 * SetObjectRot) — registry objek server-side.
 *
 * Server bersifat otoritatif untuk daftar objek. Objek di-broadcast ke client
 * yang se-virtual-world (objek punya vw sendiri; vw -1 = terlihat di semua
 * world ala SA-MP SetObjectVirtualWorld(objectid, -1)).
 *
 * MoveObject ala SA-MP: server menerima target + speed (unit/detik), mengirim
 * 0x2E OBJ_MOVE ke viewer, lalu menggerakkan posisi otoritatif di object_tick()
 * sampai tiba; saat tiba, event Lua onObjectMoved(objid) menyala.
 *
 * Protokol (server -> client, lihat PROTOCOL.md):
 *   0x2C OBJ_SPAWN:   u16 objid | u32 modelid | f32 x,y,z | f32 rx,ry,rz
 *                     | f32 drawdist | i32 vw
 *   0x2D OBJ_DESPAWN: u16 objid
 *   0x2E OBJ_MOVE:    u16 objid | f32 tx,ty,tz | f32 speed | f32 trx,try,trz
 *                     (trx/try/trz = -1000.0 berarti rotasi tak berubah)
 *   0x2F OBJ_UPDATE:  u16 objid | f32 x,y,z | f32 rx,ry,rz
 *                     (set instan ala SetObjectPos/SetObjectRot)
 */
#pragma once

#include "romp_server.h"

#define OBJECT_MAX        1000   /* maks objek aktif (batas default SA-MP) */
#define OBJECT_NO_ROT  -1000.0f  /* sentinel rotasi "tak berubah" ala SA-MP */
#define OBJECT_DRAW_DEFAULT 300.0f /* draw distance default ala SA-MP */

typedef struct GameObject {
    int      id;                  /* objid, mulai dari 1 */
    uint32_t modelid;             /* model id GTA */
    float    x, y, z;            /* posisi dunia (otoriatif server) */
    float    rx, ry, rz;         /* rotasi derajat */
    float    drawdist;            /* jarak render client */
    int32_t  vw;                 /* virtual world; -1 = semua world */
    /* state MoveObject */
    int      moving;              /* 1 = sedang bergerak */
    float    sx, sy, sz;         /* posisi awal gerakan */
    float    tx, ty, tz;         /* posisi target */
    float    srx, sry, srz;      /* rotasi awal */
    float    trx, try_, trz;      /* rotasi target (OBJECT_NO_ROT = tetap) */
    float    speed;               /* unit/detik */
    uint64_t move_start_ms;       /* waktu mulai (now_ms) */
    float    move_dist;           /* jarak total gerakan */
    struct GameObject *next;
} GameObject;

/* ---- Registry ---- */
GameObject *object_by_id(int id);
GameObject *object_first(void);  /* iterasi: NULL = tidak ada */
int         object_count(void);
/* Buat objek. drawdist <= 0 = default. Kembalikan objid (>0) atau -1.
 * Broadcast 0x2C ke viewer. */
int object_create(uint32_t modelid, float x, float y, float z,
                  float rx, float ry, float rz, float drawdist);
/* Hancurkan objek (broadcast 0x2D ke viewer). 1 = sukses. */
int object_destroy(int id);
/* Gerakkan objek ala SA-MP MoveObject: speed > 0 (unit/detik).
 * trx/try/trz = OBJECT_NO_ROT berarti rotasi tak berubah.
 * Broadcast 0x2E ke viewer; posisi otoritatif dimajukan di object_tick().
 * 1 = sukses. */
int object_move(int id, float tx, float ty, float tz, float speed,
                float trx, float try_, float trz);
/* Hentikan gerakan (broadcast 0x2F posisi saat ini). 1 = sukses. */
int object_stop(int id);
/* 1 bila objek sedang bergerak (MoveObject belum tiba). */
int object_is_moving(int id);
/* Set posisi instan (hentikan gerakan bila ada; broadcast 0x2F). */
int object_set_pos(int id, float x, float y, float z);
/* Set rotasi instan (broadcast 0x2F). */
int object_set_rot(int id, float rx, float ry, float rz);
/* Pindahkan objek ke virtual world lain (ala SA-MP SetObjectVirtualWorld;
 * -1 = semua world). Broadcast 0x2D ke viewer lama + 0x2C ke viewer baru. */
int object_set_vw(int id, int32_t vw);
/* 1 bila pemain c melihat objek o (vw cocok atau objek vw -1). */
int object_visible_to(const GameObject *o, const Client *c);

/* ---- Tick & sync jaringan ---- */
/* Majukan semua gerakan MoveObject; panggil tiap iterasi loop utama (~100ms).
 * Saat gerakan tiba: posisi = target, moving = 0, event onObjectMoved. */
void object_tick(uint64_t tnow_ms);
/* Stream objek ke pemain yang baru join: hanya yang terlihat. Bila objek
 * sedang bergerak, kirim 0x2C (posisi saat ini) + 0x2E (sisa gerakan). */
void object_sync_join(Client *c);
/* Resync objek saat pemain pindah vw: despawn yang tak terlihat lagi (ke c)
 * + spawn yang baru terlihat (ke c). Dipakai player_set_world. */
void object_sync_world(Client *c, uint32_t old_vw);
