/*
 * RO-MP 3D text label ala SA-MP (Create3DTextLabel) — registry label server-side.
 *
 * Server bersifat otoritatif untuk daftar label (buat/hapus/ubah teks/posisi/
 * attach). Label di-broadcast ke client yang se-virtual-world (label punya
 * vw sendiri bila statis; bila di-attach, vw mengikuti entity-nya).
 *
 * Label yang di-attach ke pemain/kendaraan TIDAK di-sync posisinya secara
 * periodik: paket 0x28 membawa attach_type + attach_id + offset, dan client
 * menghitung posisi label dari posisi entity yang sudah di-sync (0x11/0x13).
 *
 * Ala SA-MP, label yang di-attach ikut terhapus saat pemain disconnect atau
 * kendaraan dihancurkan.
 *
 * Protokol (server -> client, lihat PROTOCOL.md):
 *   0x28 LABEL_SPAWN: u16 labelid | u8 attach_type (0=statis,1=pemain,2=kend.)
 *                     | u16 attach_id (pid/vid; 0xFFFF bila statis)
 *                     | f32 x,y,z (statis: posisi dunia; attached: offset)
 *                     | f32 drawdist | u32 color (0xRRGGBBAA)
 *                     | u8 textlen | char text[textlen]
 *   0x29 LABEL_DESPAWN: u16 labelid
 *   0x2A LABEL_TEXT:  u16 labelid | u32 color | u8 textlen | char text[textlen]
 *   0x2B LABEL_POS:   u16 labelid | f32 x,y,z (hanya untuk label statis;
 *                     client mengabaikan untuk label yang di-attach)
 */
#pragma once

#include "romp_server.h"

#define LABEL_MAX       1024   /* maks label aktif */
#define LABEL_TEXT_MAX  200    /* maks byte teks (u8 textlen) */

#define LABEL_ATTACH_NONE    0
#define LABEL_ATTACH_PLAYER  1
#define LABEL_ATTACH_VEHICLE 2

typedef struct Label3D {
    int      id;                  /* labelid, mulai dari 1 */
    char     text[LABEL_TEXT_MAX + 1];
    uint32_t color;               /* 0xRRGGBBAA ala SA-MP */
    float    x, y, z;            /* statis: posisi dunia; attached: offset */
    float    drawdist;            /* jarak render */
    uint32_t vw;                 /* virtual world; hanya dipakai bila statis */
    int      attach_type;        /* LABEL_ATTACH_* */
    int      attach_id;          /* pid / vid, -1 bila statis */
    struct Label3D *next;
} Label3D;

/* ---- Registry ---- */
Label3D *label_by_id(int id);
Label3D *label_first(void);      /* iterasi: NULL = tidak ada */
int      label_count(void);
/* Buat label statis. text dipotong ke LABEL_TEXT_MAX byte. drawdist <= 0
 * diganti default 100. Kembalikan labelid (>0) atau -1 bila gagal.
 * Broadcast 0x28 ke viewer vw. */
int label_create(const char *text, uint32_t color, float x, float y, float z,
                 float drawdist, uint32_t vw);
/* Hancurkan label (broadcast 0x29 ke viewer). 1 = sukses. */
int label_destroy(int id);
/* Ubah teks & warna (broadcast 0x2A ke viewer). 1 = sukses. */
int label_set_text(int id, uint32_t color, const char *text);
/* Pindahkan label statis (broadcast 0x2B ke viewer). Gagal (0) bila
 * label di-attach — pakai attach ulang untuk mengubah offset. */
int label_set_pos(int id, float x, float y, float z);
/* Attach label ke pemain / kendaraan dengan offset (ox,oy,oz).
 * Broadcast 0x29 (vw lama) + 0x28 (vw entity). 1 = sukses. */
int label_attach_player(int id, int pid, float ox, float oy, float oz);
int label_attach_vehicle(int id, int vid, float ox, float oy, float oz);
/* Virtual world efektif label: vw entity bila di-attach, vw sendiri bila
 * statis. */
uint32_t label_eff_vw(Label3D *l);

/* ---- Sync jaringan ---- */
/* Kirim 0x28 semua label yang terlihat ke client baru (dipakai saat join). */
void label_sync_join(Client *c);
/* Resync label saat pemain pindah vw: despawn label vw lama (ke c) +
 * spawn label vw baru (ke c); label yang di-attach ke c ikut pindah dan
 * di-resync ke semua viewer. Dipakai player_set_world. */
void label_sync_world(Client *c, uint32_t old_vw);
/* Resync label yang di-attach ke kendaraan saat kendaraan pindah vw.
 * Dipakai vehicle_set_vw. */
void label_sync_vehicle_world(int vid, uint32_t old_vw);
/* Hapus semua label yang di-attach ke pemain (dipakai saat disconnect).
 * Broadcast 0x29 ke viewer vw pemain. */
void label_on_player_leave(int pid);
/* Hapus semua label yang di-attach ke kendaraan (dipakai saat kendaraan
 * dihancurkan). Broadcast 0x29 ke viewer vw kendaraan. */
void label_on_vehicle_destroy(int vid);
