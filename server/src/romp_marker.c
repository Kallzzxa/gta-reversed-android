/*
 * RO-MP player marker ala SA-MP (ShowPlayerMarkers /
 * SetPlayerMarkerForPlayer / GetPlayerColor) — implementasi.
 * Lihat romp_marker.h untuk desain protokol & API.
 */
#include "romp_marker.h"

#include <stdlib.h>
#include <string.h>

/* ---- Palet warna otomatis ala SA-MP (0xRRGGBBAA, terang & kontras) ---- */
static const uint32_t MARKER_PALETTE[] = {
    0xFF0000FF, /* merah        */
    0x00FF00FF, /* hijau        */
    0x0000FFFF, /* biru         */
    0xFFFF00FF, /* kuning       */
    0xFF00FFFF, /* magenta      */
    0x00FFFFFF, /* cyan         */
    0xFF8000FF, /* oranye       */
    0x8000FFFF, /* ungu         */
    0xFF0080FF, /* pink         */
    0x80FF00FF, /* hijau limau  */
    0x0080FFFF, /* biru langit  */
    0xFF4040FF, /* merah muda   */
    0x40FF40FF, /* hijau muda   */
    0x4040FFFF, /* biru muda    */
    0xFFFFFF00 + 0xFF, /* putih (0xFFFFFFFF) */
    0x808080FF, /* abu-abu      */
};
#define MARKER_PALETTE_N (sizeof(MARKER_PALETTE) / sizeof(MARKER_PALETTE[0]))

/* Mode marker global (default STREAMED ala SA-MP). */
static int g_marker_mode = MARKER_MODE_STREAMED;

/* ---------------- Helper frame ---------------- */

static void mput_u32(uint8_t *b, uint32_t v) {
    b[0] = (uint8_t)(v & 0xFF);
    b[1] = (uint8_t)((v >> 8) & 0xFF);
    b[2] = (uint8_t)((v >> 16) & 0xFF);
    b[3] = (uint8_t)((v >> 24) & 0xFF);
}

/* Bangun frame 0x44 MARKER_SET. Kembalikan panjang (6). */
static size_t marker_set_frame(int pid, uint32_t color, uint8_t *b) {
    b[0] = P_MARKER_SET;
    b[1] = (uint8_t)pid;
    mput_u32(b + 2, color);
    return 6;
}

/* Kirim 0x44 (warna efektif target menurut viewer) ke satu viewer.
 * no-op untuk NPC via client_send_frame. */
static void marker_send_to(const Client *viewer, const Client *target) {
    uint8_t b[6];
    client_send_frame((Client *)viewer, b,
                      marker_set_frame(target->id,
                                       marker_effective(viewer, target), b));
}

/* Kirim 0x44 untuk target ke SEMUA viewer yang melihatnya
 * (mode != OFF). Dipakai setelah warna milik target berubah, saat
 * pemain baru join (memberitahu viewer lama), & saat mode dinyalakan. */
void marker_broadcast_color(const Client *target, const Client *except) {
    if (g_marker_mode == MARKER_MODE_OFF) return;
    for (Client *v = g_clients; v; v = v->next) {
        if (!v->handshaked || v->dead || v->is_npc || v == except) continue;
        if (!client_visible(v, target)) continue;
        marker_send_to(v, target);
    }
}

/* Cari override viewer -> showpid. Kembalikan index, atau -1. */
static int marker_ov_find(const Client *viewer, int showpid) {
    if (!viewer) return -1;
    for (int i = 0; i < viewer->marker_novs; i++) {
        if (viewer->marker_ovs[i].pid == showpid) return i;
    }
    return -1;
}

/* ---------------- API inti ---------------- */

int marker_valid_mode(int mode) {
    return mode >= MARKER_MODE_OFF && mode <= MARKER_MODE_STREAMED;
}

int marker_get_mode(void) {
    return g_marker_mode;
}

int marker_set_mode(int mode) {
    if (!marker_valid_mode(mode)) return 0;
    if (g_marker_mode == mode) return 1;
    g_marker_mode = mode;
    uint8_t b[2];
    b[0] = P_MARKER_MODE;
    b[1] = (uint8_t)mode;
    broadcast_frame(b, 2, NULL);
    /* Mode dinyalakan kembali: kirim ulang semua warna efektif agar client
     * yang tadi menyembunyikan marker menggambar lagi. */
    if (mode != MARKER_MODE_OFF) {
        for (Client *t = g_clients; t; t = t->next) {
            if (!t->handshaked || t->dead) continue;
            marker_broadcast_color(t, NULL);
        }
    }
    log_msg("[marker] mode -> %d", mode);
    return 1;
}

void marker_init(Client *c) {
    if (!c) return;
    unsigned id = c->id < 0 ? 0 : (unsigned)c->id;
    c->marker_color = MARKER_PALETTE[id % MARKER_PALETTE_N];
    c->marker_ovs = NULL;
    c->marker_novs = 0;
    c->marker_capovs = 0;
}

void marker_on_player_leave(Client *c) {
    if (!c) return;
    int id = c->id;
    /* Hapus override pemain lain yang menarget id ini. */
    for (Client *v = g_clients; v; v = v->next) {
        if (v == c || !v->marker_ovs) continue;
        int w = 0;
        for (int i = 0; i < v->marker_novs; i++) {
            if (v->marker_ovs[i].pid == id) continue; /* buang */
            if (w != i) v->marker_ovs[w] = v->marker_ovs[i];
            w++;
        }
        v->marker_novs = w;
    }
    /* Bebaskan array override milik pemain yang keluar (idempoten). */
    free(c->marker_ovs);
    c->marker_ovs = NULL;
    c->marker_novs = 0;
    c->marker_capovs = 0;
}

int marker_set_color(Client *c, uint32_t color) {
    if (!c || !c->handshaked || c->dead) return 0;
    c->marker_color = color;
    marker_broadcast_color(c, NULL);
    log_msg("[marker] %s (id %d): warna -> 0x%08X", c->name, c->id, color);
    return 1;
}

uint32_t marker_get_color(const Client *c) {
    if (!c || !c->handshaked || c->dead) return 0;
    return c->marker_color;
}

uint32_t marker_effective(const Client *viewer, const Client *target) {
    if (!target) return 0;
    int i = marker_ov_find(viewer, target->id);
    if (i >= 0) return viewer->marker_ovs[i].color;
    return target->marker_color;
}

int marker_set_for(Client *viewer, int showpid, uint32_t color) {
    if (!viewer || !viewer->handshaked || viewer->dead) return 0;
    if (showpid < 0) return 0;
    Client *t = client_by_id(showpid);
    if (!t || !t->handshaked || t->dead) return 0;

    int i = marker_ov_find(viewer, showpid);
    if (i >= 0) {
        viewer->marker_ovs[i].color = color; /* timpa */
    } else {
        if (viewer->marker_novs >= viewer->marker_capovs) {
            int ncap = viewer->marker_capovs ? viewer->marker_capovs * 2 : 4;
            MarkerOverride *no =
                (MarkerOverride *)realloc(viewer->marker_ovs,
                                          (size_t)ncap * sizeof(MarkerOverride));
            if (!no) return 0;
            viewer->marker_ovs = no;
            viewer->marker_capovs = ncap;
        }
        viewer->marker_ovs[viewer->marker_novs].pid = showpid;
        viewer->marker_ovs[viewer->marker_novs].color = color;
        viewer->marker_novs++;
    }

    /* Kirim 0x44 hanya ke viewer ini bila ia melihat target & mode aktif
     * (no-op untuk viewer NPC via client_send_frame). */
    if (g_marker_mode != MARKER_MODE_OFF && client_visible(viewer, t))
        marker_send_to(viewer, t);

    log_msg("[marker] viewer %s (id %d): %d -> 0x%08X",
            viewer->name, viewer->id, showpid, color);
    return 1;
}

int marker_clear_for(Client *viewer, int showpid) {
    if (!viewer || !viewer->handshaked || viewer->dead) return 0;
    int i = marker_ov_find(viewer, showpid);
    if (i < 0) return 0;
    for (int j = i + 1; j < viewer->marker_novs; j++)
        viewer->marker_ovs[j - 1] = viewer->marker_ovs[j];
    viewer->marker_novs--;
    /* Kembalikan tampilan ke warna milik target. */
    Client *t = client_by_id(showpid);
    if (t && t->handshaked && !t->dead &&
        g_marker_mode != MARKER_MODE_OFF && client_visible(viewer, t))
        marker_send_to(viewer, t);
    return 1;
}

int marker_clear_all_for(Client *viewer) {
    if (!viewer) return 0;
    int n = viewer->marker_novs;
    viewer->marker_novs = 0;
    return n;
}

void marker_sync_join(Client *c) {
    if (!c || !c->handshaked) return;
    uint8_t b[2];
    b[0] = P_MARKER_MODE;
    b[1] = (uint8_t)g_marker_mode;
    client_send_frame(c, b, 2);
    if (g_marker_mode == MARKER_MODE_OFF) return;
    for (Client *t = g_clients; t; t = t->next) {
        if (!t->handshaked || t->dead) continue;
        if (!client_visible(c, t)) continue;
        marker_send_to(c, t);
    }
}

void marker_sync_world(Client *c) {
    if (!c || !c->handshaked || c->dead) return;
    if (g_marker_mode == MARKER_MODE_OFF) return;
    /* Dua arah, idempoten: untuk tiap pasangan yang sekarang saling
     * terlihat, kirim ulang warna efektif. */
    for (Client *o = g_clients; o; o = o->next) {
        if (!o->handshaked || o->dead || o == c || o->is_npc) continue;
        if (!client_visible(c, o)) continue;
        marker_send_to(o, c);
        if (!c->is_npc) marker_send_to(c, o);
    }
}
