/*
 * RO-MP Dedicated Server v1
 * SA-MP-like multiplayer server untuk gta-reversed-android (project gabut).
 *
 * Build: lihat Makefile (make linux / make windows).
 * Gamemode scripting: Lua, lihat gamemode.lua (ala FiveM).
 *
 * Protokol: lihat ../PROTOCOL.md
 */
#define _CRT_SECURE_NO_WARNINGS

#include "romp_server.h"
#include "romp_lua.h"
#include "romp_vehicle.h"
#include "romp_label.h"
#include "romp_weapon.h"
#include "romp_anticheat.h"
#include "romp_sqlite.h"
#include "romp_timer.h"

#include <time.h>
#include <pthread.h>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#define SOCKERR WSAGetLastError()
#else
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#include <netdb.h>
#define SOCKERR errno
#endif

/* ---------------- Konfigurasi ---------------- */
Config g_cfg = {
    7777,
    "RO-MP Server",
    "Selamat datang di RO-MP!",
    50,
    "",
    "gamemode.lua",
    "",     /* filterscripts (kosong = tidak ada yang di-load otomatis) */
    1,      /* query */
    0,      /* announce */
    "",     /* master_host */
    7778,   /* master_port */
    1,      /* lan_discovery */
    1,      /* rcon */
    "",     /* rcon_password */
    1,      /* anticheat */
    3,      /* ac_maxwarnings (0 = tidak pernah kick otomatis) */
    ""      /* database (kosong = fitur SQLite mati) */
};

static void cfg_load(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0') continue;
        char key[64], val[448];
        val[0] = '\0';
        if (sscanf(p, "%63s %447[^\r\n]", key, val) < 1) continue;
        /* trim trailing space dari val */
        size_t vl = strlen(val);
        while (vl > 0 && (val[vl-1] == ' ' || val[vl-1] == '\t')) val[--vl] = '\0';
        if (!strcmp(key, "port")) g_cfg.port = atoi(val);
        else if (!strcmp(key, "hostname")) { strncpy(g_cfg.hostname, val, sizeof(g_cfg.hostname)-1); }
        else if (!strcmp(key, "motd")) { strncpy(g_cfg.motd, val, sizeof(g_cfg.motd)-1); }
        else if (!strcmp(key, "maxplayers")) {
            g_cfg.maxplayers = atoi(val);
            if (g_cfg.maxplayers < 1) g_cfg.maxplayers = 1;
            if (g_cfg.maxplayers > 120) g_cfg.maxplayers = 120;
        }
        else if (!strcmp(key, "password")) { strncpy(g_cfg.password, val, sizeof(g_cfg.password)-1); }
        else if (!strcmp(key, "gamemode")) { strncpy(g_cfg.gamemode, val, sizeof(g_cfg.gamemode)-1); }
        else if (!strcmp(key, "filterscripts")) { strncpy(g_cfg.filterscripts, val, sizeof(g_cfg.filterscripts)-1); }
        else if (!strcmp(key, "query")) g_cfg.query = atoi(val) ? 1 : 0;
        else if (!strcmp(key, "announce")) g_cfg.announce = atoi(val) ? 1 : 0;
        else if (!strcmp(key, "master_host")) { strncpy(g_cfg.master_host, val, sizeof(g_cfg.master_host)-1); }
        else if (!strcmp(key, "master_port")) g_cfg.master_port = atoi(val);
        else if (!strcmp(key, "lan_discovery")) g_cfg.lan_discovery = atoi(val) ? 1 : 0;
        else if (!strcmp(key, "rcon")) g_cfg.rcon = atoi(val) ? 1 : 0;
        else if (!strcmp(key, "rcon_password")) { strncpy(g_cfg.rcon_password, val, sizeof(g_cfg.rcon_password)-1); }
        else if (!strcmp(key, "anticheat")) g_cfg.anticheat = atoi(val) ? 1 : 0;
        else if (!strcmp(key, "ac_maxwarnings")) {
            g_cfg.ac_maxwarnings = atoi(val);
            if (g_cfg.ac_maxwarnings < 0) g_cfg.ac_maxwarnings = 0;
        }
        else if (!strcmp(key, "database")) { strncpy(g_cfg.database, val, sizeof(g_cfg.database)-1); }
    }
    fclose(f);
}

/* ---------------- Daftar ban IP (in-memory) ---------------- */
typedef struct BanEntry { char ip[48]; struct BanEntry *next; } BanEntry;
static BanEntry *g_bans = NULL;

void ban_add(const char *ip) {
    if (!ip || !ip[0] || ban_check(ip)) return;
    BanEntry *b = (BanEntry*)calloc(1, sizeof(BanEntry));
    strncpy(b->ip, ip, sizeof(b->ip) - 1);
    b->next = g_bans; g_bans = b;
    log_msg("IP %s di-ban.", ip);
}
void ban_remove(const char *ip) {
    BanEntry **pp = &g_bans;
    while (*pp) {
        if (!strcmp((*pp)->ip, ip)) {
            BanEntry *t = *pp; *pp = t->next; free(t);
            log_msg("IP %s di-unban.", ip);
            return;
        }
        pp = &(*pp)->next;
    }
}
void ban_clear(void) {
    BanEntry *b = g_bans;
    while (b) { BanEntry *t = b; b = b->next; free(t); }
    g_bans = NULL;
    log_msg("Semua ban IP dihapus.");
}
int ban_check(const char *ip) {
    for (BanEntry *b = g_bans; b; b = b->next)
        if (!strcmp(b->ip, ip)) return 1;
    return 0;
}

int ban_get(int idx, char *ip_out, size_t n) {
    int i = 0;
    for (BanEntry *b = g_bans; b; b = b->next, i++) {
        if (i == idx) {
            strncpy(ip_out, b->ip, n - 1);
            ip_out[n - 1] = '\0';
            return 1;
        }
    }
    return 0;
}

/* ---------------- Logging ---------------- */
static FILE *g_logf = NULL;

void log_msg(const char *fmt, ...) {
    char buf[1024];
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char ts[32];
    strftime(ts, sizeof(ts), "%H:%M:%S", tm);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    printf("[%s] %s\n", ts, buf);
    fflush(stdout);
    if (g_logf) { fprintf(g_logf, "[%s] %s\n", ts, buf); fflush(g_logf); }
}

/* ---------------- Util socket ---------------- */
static int sock_nonblock(SOCKET s) {
#ifdef _WIN32
    u_long m = 1;
    return ioctlsocket(s, FIONBIO, &m);
#else
    int f = fcntl(s, F_GETFL, 0);
    return fcntl(s, F_SETFL, f | O_NONBLOCK);
#endif
}

static void put_u16(uint8_t *b, uint16_t v) { b[0] = v & 0xFF; b[1] = (v >> 8) & 0xFF; }
static void put_u32(uint8_t *b, uint32_t v) {
    b[0] = v & 0xFF; b[1] = (v >> 8) & 0xFF;
    b[2] = (v >> 16) & 0xFF; b[3] = (v >> 24) & 0xFF;
}
static void put_f32(uint8_t *b, float v) { uint32_t u; memcpy(&u, &v, 4); put_u32(b, u); }
static uint16_t get_u16(const uint8_t *b) { return (uint16_t)(b[0] | (b[1] << 8)); }
static uint32_t get_u32(const uint8_t *b) {
    return (uint32_t)(b[0] | (b[1] << 8) | (b[2] << 16) | ((uint32_t)b[3] << 24));
}
static float get_f32(const uint8_t *b) { uint32_t u = get_u32(b); float f; memcpy(&f, &u, 4); return f; }

uint64_t now_ms(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

/* ---------------- Client ---------------- */
Client *g_clients = NULL;
static volatile sig_atomic_t g_running = 1;
static int g_next_id = 0;

int client_count(void) {
    int n = 0;
    for (Client *c = g_clients; c; c = c->next) if (c->handshaked) n++;
    return n;
}

Client *client_by_id(int id) {
    for (Client *c = g_clients; c; c = c->next)
        if (c->handshaked && c->id == id) return c;
    return NULL;
}

Client *client_by_name(const char *name) {
    for (Client *c = g_clients; c; c = c->next)
        if (c->handshaked && !strcmp(c->name, name)) return c;
    return NULL;
}

static int alloc_id(void) {
    for (int tries = 0; tries < 1000; tries++) {
        int id = g_next_id++;
        if (g_next_id > 9999) g_next_id = 0;
        if (!client_by_id(id)) return id;
    }
    return -1;
}

static int valid_nick(const char *n, size_t len) {
    if (len < 3 || len > 24) return 0;
    for (size_t i = 0; i < len; i++) {
        char c = n[i];
        if (!(c >= 'A' && c <= 'Z') && !(c >= 'a' && c <= 'z') &&
            !(c >= '0' && c <= '9') && c != '_' && c != '-' &&
            c != '[' && c != ']')
            return 0;
    }
    return 1;
}

/* ---------------- Kirim paket ---------------- */
static int send_all(SOCKET s, const uint8_t *b, size_t n) {
    size_t off = 0;
    while (off < n) {
#ifdef _WIN32
        int r = send(s, (const char*)b + off, (int)(n - off), 0);
#else
        ssize_t r = send(s, b + off, n - off, MSG_NOSIGNAL);
#endif
        if (r == SOCKET_ERROR) {
            int e = SOCKERR;
#ifdef _WIN32
            if (e == WSAEWOULDBLOCK) { continue; }
#else
            if (e == EAGAIN || e == EWOULDBLOCK || e == EINTR) { continue; }
#endif
            return -1;
        }
        off += (size_t)r;
    }
    return 0;
}

/* b = payload (sudah termasuk type di b[0]); n = panjang payload */
static int send_frame(Client *c, const uint8_t *payload, size_t n) {
    /* NPC tak punya socket: kirim ke NPC selalu jadi no-op (sukses). */
    if (c->is_npc) return 0;
    uint8_t hdr[2];
    put_u16(hdr, (uint16_t)n);
    if (send_all(c->sock, hdr, 2) < 0) return -1;
    return send_all(c->sock, payload, n);
}

/* Wrapper non-static untuk scripting layer. */
int client_send_frame(Client *c, const uint8_t *payload, size_t n) {
    return send_frame(c, payload, n);
}

static void send_reject(Client *c, const char *reason) {
    uint8_t b[256];
    size_t rl = strlen(reason);
    if (rl > 200) rl = 200;
    b[0] = P_REJECT;
    b[1] = (uint8_t)rl;
    memcpy(b + 2, reason, rl);
    send_frame(c, b, 2 + rl);
}

static void broadcast(const uint8_t *payload, size_t n, Client *except) {
    for (Client *c = g_clients; c; c = c->next) {
        if (!c->handshaked || c->dead || c->is_npc || c == except) continue;
        if (send_frame(c, payload, n) < 0) c->dead = 1;
    }
}

/* Wrapper non-static untuk modul lain (mis. romp_vehicle.c). */
void broadcast_frame(const uint8_t *payload, size_t n, Client *except) {
    broadcast(payload, n, except);
}

/* ---- Interior & virtual world ala SA-MP ----
 * Dua pemain saling "terlihat" (posisi, chat, tembakan, entity join/leave)
 * hanya bila virtual world DAN interior mereka sama. Mirip SA-MP. */
int client_visible(const Client *a, const Client *b) {
    if (!a || !b) return 0;
    return a->vw == b->vw && a->interior == b->interior;
}

/* Broadcast hanya ke client yang terlihat oleh `ref`. */
static void broadcast_scoped(const uint8_t *payload, size_t n,
                             Client *ref, Client *except) {
    for (Client *c = g_clients; c; c = c->next) {
        if (!c->handshaked || c->dead || c->is_npc || c == except) continue;
        if (!client_visible(ref, c)) continue;
        if (send_frame(c, payload, n) < 0) c->dead = 1;
    }
}

void broadcast_frame_ref(const uint8_t *payload, size_t n,
                         Client *ref, Client *except) {
    if (ref) broadcast_scoped(payload, n, ref, except);
    else broadcast(payload, n, except);
}

void broadcast_frame_vw(const uint8_t *payload, size_t n,
                        Client *except, uint32_t vw) {
    for (Client *c = g_clients; c; c = c->next) {
        if (!c->handshaked || c->dead || c->is_npc || c == except) continue;
        if (c->vw != vw) continue;
        if (send_frame(c, payload, n) < 0) c->dead = 1;
    }
}

/* Paket 0x25: beritahu pemain interior & vw-nya saat ini. */
void send_int_vw(Client *c) {
    uint8_t b[6];
    b[0] = P_INT_VW;
    b[1] = c->interior;
    b[2] = (uint8_t)(c->vw & 0xFF);
    b[3] = (uint8_t)((c->vw >> 8) & 0xFF);
    b[4] = (uint8_t)((c->vw >> 16) & 0xFF);
    b[5] = (uint8_t)((c->vw >> 24) & 0xFF);
    if (send_frame(c, b, 6) < 0) c->dead = 1;
}

/* Ganti interior & vw pemain + resync streaming. Dipakai API Lua.
 * - Event Lua onPlayerInteriorChange / onPlayerVirtualWorldChange.
 * - Pemain lain yang tak terlihat lagi -> P_LEAVE; yang baru terlihat -> P_JOIN.
 * - Kendaraan: resync bila vw berubah; bila pemain menyopir, kendaraan ikut
 *   pindah vw bersamanya. */
static size_t build_join(uint8_t *b, const Client *who); /* didefinisikan di bawah */
void player_set_world(Client *c, int interior, uint32_t vw) {
    if (!c || !c->handshaked) return;
    if (interior < 0) interior = 0;
    if (interior > 255) interior = 255;
    uint8_t old_int = c->interior;
    uint32_t old_vw = c->vw;
    if (old_int == (uint8_t)interior && old_vw == vw) return;
    c->interior = (uint8_t)interior;
    c->vw = vw;
    send_int_vw(c);
    if (old_int != c->interior)
        lua_on_player_interior_change(c->id, old_int, c->interior);
    if (old_vw != vw)
        lua_on_player_vw_change(c->id, old_vw, vw);
    /* Resync streaming dua arah: c hilang/muncul di pandangan pemain lain,
     * dan sebaliknya. */
    for (Client *o = g_clients; o; o = o->next) {
        if (!o->handshaked || o->dead || o == c) continue;
        int was = (old_vw == o->vw && old_int == o->interior);
        int now = client_visible(c, o);
        if (was == now) continue;
        if (!now) {
            /* c tak terlihat lagi oleh o, dan sebaliknya */
            uint8_t bl[4];
            bl[0] = P_LEAVE;
            put_u16(bl + 1, (uint16_t)c->id);
            bl[3] = LEAVE_QUIT;
            if (send_frame(o, bl, 4) < 0) o->dead = 1;
            put_u16(bl + 1, (uint16_t)o->id);
            if (send_frame(c, bl, 4) < 0) c->dead = 1;
        } else {
            /* c & o baru saling terlihat */
            uint8_t bj[64];
            size_t jn = build_join(bj, c);
            if (send_frame(o, bj, jn) < 0) o->dead = 1;
            jn = build_join(bj, o);
            if (send_frame(c, bj, jn) < 0) c->dead = 1;
        }
    }
    /* Resync kendaraan bila vw berubah: despawn vw lama & spawn vw baru dulu,
     * lalu bila pemain menyopir, kendaraan ikut pindah vw bersamanya. */
    if (old_vw != vw) {
        vehicle_sync_world(c, old_vw);
        label_sync_world(c, old_vw);
        if (c->vehicle_id >= 0) {
            Vehicle *v = vehicle_by_id(c->vehicle_id);
            if (v && v->driver == c->id) vehicle_set_vw(v->id, vw);
        }
    }
    log_msg("[vw] %s (id %d): interior %d->%d, vw %u->%u",
            c->name, c->id, old_int, c->interior, old_vw, vw);
}

/* ---------------- Chat ---------------- */
void chat_broadcast(uint16_t pid, uint8_t kind, const char *msg) {
    uint8_t b[512];
    size_t ml = strlen(msg);
    if (ml > 200) ml = 200;
    b[0] = P_CHAT_MSG;
    put_u16(b + 1, pid);
    b[3] = kind;
    b[4] = (uint8_t)ml;
    memcpy(b + 5, msg, ml);
    broadcast(b, 5 + ml, NULL);
    log_msg("[chat] %s", msg);
}

void chat_to(Client *c, uint16_t pid, uint8_t kind, const char *msg) {
    uint8_t b[512];
    size_t ml = strlen(msg);
    if (ml > 200) ml = 200;
    b[0] = P_CHAT_MSG;
    put_u16(b + 1, pid);
    b[3] = kind;
    b[4] = (uint8_t)ml;
    memcpy(b + 5, msg, ml);
    if (send_frame(c, b, 5 + ml) < 0) c->dead = 1;
}

/* Chat ala SA-MP: chat pemain (/me, chat biasa) hanya sampai ke pemain yang
 * se-vw & se-interior dengan `ref`. Pengumuman server (join/kick/dsb) tetap
 * global via chat_broadcast. */
void chat_broadcast_scoped(Client *ref, uint16_t pid, uint8_t kind, const char *msg) {
    uint8_t b[512];
    size_t ml = strlen(msg);
    if (ml > 200) ml = 200;
    b[0] = P_CHAT_MSG;
    put_u16(b + 1, pid);
    b[3] = kind;
    b[4] = (uint8_t)ml;
    memcpy(b + 5, msg, ml);
    broadcast_scoped(b, 5 + ml, ref, NULL);
    log_msg("[chat] %s", msg);
}

void server_say(const char *msg) {
    char full[256];
    snprintf(full, sizeof(full), "*** %s", msg);
    chat_broadcast(CHAT_SERVER, KIND_INFO, full);
}

/* ---------------- Join / Leave ---------------- */
/* Bangun paket 0x22 PLAYER_JOIN untuk `who`.
 * v1.1: 1 byte flags ditambahkan setelah nama (bit0 = 1 kalau NPC).
 * Client lama yang parse nama pakai namelen boleh abaikan byte tambahan ini. */
static size_t build_join(uint8_t *b, const Client *who) {
    size_t nl = strlen(who->name);
    if (nl > 24) nl = 24;
    b[0] = P_JOIN;
    put_u16(b + 1, (uint16_t)who->id);
    b[3] = (uint8_t)nl;
    memcpy(b + 4, who->name, nl);
    b[4 + nl] = who->is_npc ? 1 : 0;
    return 5 + nl;
}

static void send_player_list(Client *c) {
    uint8_t b[4096];
    b[0] = P_LIST;
    int n = 0;
    size_t off = 2;
    for (Client *o = g_clients; o; o = o->next) {
        if (!o->handshaked || o == c || o->dead) continue;
        if (!client_visible(c, o)) continue; /* hanya yang se-vw & se-interior */
        size_t nl = strlen(o->name);
        if (off + 3 + nl > sizeof(b)) break;
        put_u16(b + off, (uint16_t)o->id); off += 2;
        b[off++] = (uint8_t)nl;
        memcpy(b + off, o->name, nl); off += nl;
        n++;
    }
    b[1] = (uint8_t)n;
    send_frame(c, b, off);
}

static void do_join(Client *c) {
    uint8_t b[64];

    /* WELCOME */
    uint8_t w[512];
    size_t off = 0;
    w[off++] = P_WELCOME;
    put_u16(w + off, (uint16_t)c->id); off += 2;
    size_t hl = strlen(g_cfg.hostname); if (hl > 60) hl = 60;
    w[off++] = (uint8_t)hl; memcpy(w + off, g_cfg.hostname, hl); off += hl;
    size_t ml = strlen(g_cfg.motd); if (ml > 200) ml = 200;
    w[off++] = (uint8_t)ml; memcpy(w + off, g_cfg.motd, ml); off += ml;
    w[off++] = (uint8_t)g_cfg.maxplayers;
    send_frame(c, w, off);

    send_player_list(c);

    /* stream kendaraan yang sudah ada ke pemain baru */
    vehicle_sync_join(c);

    /* stream 3D text label yang sudah ada ke pemain baru */
    label_sync_join(c);

    /* beritahu yang lain (hanya yang se-vw & se-interior) */
    size_t jn = build_join(b, c);
    broadcast_scoped(b, jn, c, c);

    char msg[128];
    snprintf(msg, sizeof(msg), "*** %s (id %d) bergabung.", c->name, c->id);
    /* event Lua dulu; kalau tidak di-handle, pakai pengumuman default */
    if (!lua_on_player_join(c->id, c->name))
        chat_broadcast(CHAT_SERVER, KIND_SYS, msg);
    log_msg("%s bergabung (id %d). Online: %d", c->name, c->id, client_count());
}

static void do_leave(Client *c, int reason) {
    if (!c->handshaked) return;
    c->handshaked = 0;
    /* hapus 3D text label yang di-attach ke pemain ini (ala SA-MP) */
    label_on_player_leave(c->id);
    uint8_t b[4];
    b[0] = P_LEAVE;
    put_u16(b + 1, (uint16_t)c->id);
    b[3] = (uint8_t)reason;
    broadcast_scoped(b, 4, c, c);
    const char *rs = reason == LEAVE_KICK ? "di-kick" : reason == LEAVE_TIMEOUT ? "timeout" : "keluar";
    char msg[128];
    snprintf(msg, sizeof(msg), "*** %s (id %d) %s.", c->name, c->id, rs);
    if (!lua_on_player_leave(c->id, c->name, reason))
        chat_broadcast(CHAT_SERVER, KIND_SYS, msg);
    log_msg("%s keluar (%s). Online: %d", c->name, rs, client_count());
}

/* Kick pemain + umumkan. Dipakai console & Lua.
 * NPC di-kick lewat npc_disconnect (tak punya socket untuk ditandai dead). */
void server_kick(int id, const char *reason) {
    Client *c = client_by_id(id);
    if (!c) return;
    if (c->is_npc) {
        log_msg("Kicked NPC %s (id %d)", c->name, c->id);
        npc_disconnect(id, LEAVE_KICK);
        return;
    }
    char msg[256];
    if (reason && reason[0])
        snprintf(msg, sizeof(msg), "*** %s (id %d) di-kick (%s).", c->name, c->id, reason);
    else
        snprintf(msg, sizeof(msg), "*** %s (id %d) di-kick.", c->name, c->id);
    chat_broadcast(CHAT_SERVER, KIND_SYS, msg);
    c->dead = 1;
    c->kick_reason = LEAVE_KICK;
    log_msg("Kicked %s (id %d)", c->name, c->id);
}

/* ---------------- NPC / bot ala SA-MP (ConnectNPC) ---------------- */
/* Sambungkan NPC: koneksi virtual tanpa socket. Menempati slot pemain,
 * muncul di P_LIST & daftar pemain, dan memicu event onPlayerJoin Lua
 * (gamemode bedakan via romp.isNPC). Return id, -1 = gagal. */
int npc_connect(const char *name) {
    if (!name) return -1;
    size_t nl = strlen(name);
    if (!valid_nick(name, nl)) return -1;
    if (client_by_name(name)) return -1;
    if (client_count() >= g_cfg.maxplayers) return -1;
    int id = alloc_id();
    if (id < 0) return -1;
    Client *c = (Client *)calloc(1, sizeof(Client));
    if (!c) return -1;
    c->sock = INVALID_SOCKET;
    c->id = id;
    memcpy(c->name, name, nl); c->name[nl] = '\0';
    c->handshaked = 1;
    c->is_npc = 1;
    c->interior = 0;
    c->vw = 0;
    c->vehicle_id = -1;
    c->vehicle_seat = -1;
    c->ping_ms = -1; /* belum terukur sampai probe pertama dijawab */
    c->last_activity = now_ms();
    strncpy(c->ip, "npc", sizeof(c->ip) - 1);
    c->next = g_clients;
    g_clients = c;

    /* beritahu pemain lain (hanya yang se-vw & se-interior) */
    uint8_t b[64];
    size_t jn = build_join(b, c);
    broadcast_scoped(b, jn, c, NULL);

    char msg[128];
    snprintf(msg, sizeof(msg), "*** %s (id %d) bergabung.", c->name, c->id);
    if (!lua_on_player_join(c->id, c->name))
        chat_broadcast(CHAT_SERVER, KIND_SYS, msg);
    log_msg("[npc] %s bergabung (id %d). Online: %d", c->name, c->id,
            client_count());
    return id;
}

/* Putuskan NPC. Unlink + free langsung (tak lewat sweep_dead) supaya aman
 * bila dipanggil dari dalam event Lua (reentrancy). */
void npc_disconnect(int id, int reason) {
    Client *c = client_by_id(id);
    if (!c || !c->is_npc || !c->handshaked) return;
    vehicle_remove_player_silent(id);
    /* hapus 3D text label yang di-attach ke NPC ini (ala SA-MP) */
    label_on_player_leave(id);
    /* salin field yang dipakai setelah free */
    int nid = c->id;
    uint8_t interior = c->interior;
    uint32_t vw = c->vw;
    char name[25];
    strncpy(name, c->name, sizeof(name) - 1); name[sizeof(name) - 1] = '\0';
    Client **pp = &g_clients;
    while (*pp) {
        if (*pp == c) { *pp = c->next; break; }
        pp = &(*pp)->next;
    }
    free(c);
    /* P_LEAVE ke pemain yang dulu se-vw & se-interior dengan NPC */
    uint8_t b[4];
    b[0] = P_LEAVE;
    put_u16(b + 1, (uint16_t)nid);
    b[3] = (uint8_t)reason;
    for (Client *o = g_clients; o; o = o->next) {
        if (!o->handshaked || o->dead || o->is_npc) continue;
        if (o->vw != vw || o->interior != interior) continue;
        if (send_frame(o, b, 4) < 0) o->dead = 1;
    }
    const char *rs = reason == LEAVE_KICK ? "di-kick" :
                     reason == LEAVE_TIMEOUT ? "timeout" : "keluar";
    char msg[128];
    snprintf(msg, sizeof(msg), "*** %s (id %d) %s.", name, nid, rs);
    if (!lua_on_player_leave(nid, name, reason))
        chat_broadcast(CHAT_SERVER, KIND_SYS, msg);
    log_msg("[npc] %s keluar (%s). Online: %d", name, rs, client_count());
}

/* Pindahkan NPC + relay 0x11 P_PPOS ke pemain yang melihatnya.
 * 1 = sukses, 0 = id bukan NPC. */
int npc_set_pos(int id, float x, float y, float z) {
    Client *c = client_by_id(id);
    if (!c || !c->is_npc) return 0;
    c->pos_x = x; c->pos_y = y; c->pos_z = z;
    c->pos_known = 1;
    uint8_t b[24];
    b[0] = P_PPOS;
    put_u16(b + 1, (uint16_t)id);
    memcpy(b + 3, &x, 4);
    memcpy(b + 7, &y, 4);
    memcpy(b + 11, &z, 4);
    float heading = 0.0f;
    memcpy(b + 15, &heading, 4);
    b[19] = 0;          /* state: jalan kaki */
    b[20] = 0; b[21] = 0; /* vehicle_model = 0 */
    broadcast_scoped(b, 22, c, NULL);
    return 1;
}

/* Teleport pemain ala SA-MP SetPlayerPos (dipakai romp.setPlayerPos).
 * Posisi otoritatif server diperbarui, client target disuruh pindah via
 * paket 0x27 P_POS_SET, dan posisi baru di-relay (0x11) ke pemain yang
 * melihatnya. NPC di-routing ke npc_set_pos. 1 = sukses, 0 = id invalid. */
int player_set_pos(int id, float x, float y, float z) {
    Client *c = client_by_id(id);
    if (!c || !c->handshaked || c->dead) return 0;
    if (c->is_npc) return npc_set_pos(id, x, y, z);
    uint64_t t = now_ms();
    c->pos_x = x; c->pos_y = y; c->pos_z = z;
    c->pos_known = 1;
    c->last_pos_ms = t;
    c->pos_grace_ms = t; /* anti-cheat: abaikan lompatan paket 0x10 lama */
    uint8_t tp[13];
    tp[0] = P_POS_SET;
    memcpy(tp + 1, &x, 4);
    memcpy(tp + 5, &y, 4);
    memcpy(tp + 9, &z, 4);
    if (client_send_frame(c, tp, 13) < 0) c->dead = 1;
    uint8_t b[22];
    b[0] = P_PPOS;
    put_u16(b + 1, (uint16_t)id);
    memcpy(b + 3, &x, 4);
    memcpy(b + 7, &y, 4);
    memcpy(b + 11, &z, 4);
    float heading = 0.0f;
    memcpy(b + 15, &heading, 4);
    b[19] = 0;          /* state: jalan kaki */
    b[20] = 0; b[21] = 0; /* vehicle_model = 0 */
    broadcast_scoped(b, 22, c, c);
    return 1;
}

/* NPC kirim chat. Lewat event onPlayerChat Lua dulu (ala SA-MP: NPC chat
 * memicu OnPlayerText); kalau tak di-handle, broadcast seperti chat biasa.
 * 1 = sukses, 0 = id bukan NPC. */
int npc_send_chat(int id, const char *text) {
    Client *c = client_by_id(id);
    if (!c || !c->is_npc || !text || !text[0]) return 0;
    char msg[129];
    strncpy(msg, text, sizeof(msg) - 1); msg[sizeof(msg) - 1] = '\0';
    if (lua_on_player_chat(c->id, c->name, msg)) return 1;
    char full[256];
    snprintf(full, sizeof(full), "%s (%d): %s", c->name, c->id, msg);
    chat_broadcast_scoped(c, (uint16_t)c->id, KIND_CHAT, full);
    return 1;
}

int npc_count(void) {
    int n = 0;
    for (Client *c = g_clients; c; c = c->next)
        if (c->handshaked && c->is_npc) n++;
    return n;
}

/* ---------------- Handler paket ---------------- */
static void handle_hello(Client *c, const uint8_t *p, size_t n) {
    if (c->handshaked) return;
    if (n < 3) { send_reject(c, "Handshake invalid."); c->dead = 1; return; }
    uint8_t ver = p[0];
    uint8_t nl = p[1];
    if (ver != ROMP_PROTO_VER) { send_reject(c, "Versi client tidak cocok."); c->dead = 1; return; }
    if (nl < 3 || nl > 24 || n < (size_t)(2 + nl + 1)) {
        send_reject(c, "Nickname invalid (3-24 karakter)."); c->dead = 1; return;
    }
    if (!valid_nick((const char*)(p + 2), nl)) {
        send_reject(c, "Nickname hanya boleh A-Z 0-9 _ - [ ]."); c->dead = 1; return;
    }
    char name[25];
    memcpy(name, p + 2, nl); name[nl] = '\0';

    /* Lua: onPlayerRequestConnect(name, ip) -> true, "alasan" = tolak */
    {
        char reason[128] = "";
        if (lua_on_player_request(name, c->ip, reason, sizeof(reason))) {
            if (!reason[0]) strcpy(reason, "Ditolak gamemode.");
            send_reject(c, reason); c->dead = 1;
            log_msg("Handshake ditolak gamemode: %s (%s): %s", name, c->ip, reason);
            return;
        }
    }

    /* ban IP */
    if (ban_check(c->ip)) {
        send_reject(c, "IP kamu di-ban dari server ini.");
        c->dead = 1;
        log_msg("Handshake ditolak (banned): %s (%s)", name, c->ip);
        return;
    }

    /* password */
    size_t off = 2 + nl;
    uint8_t pwl = p[off++];
    if (off + pwl > n) { send_reject(c, "Handshake invalid."); c->dead = 1; return; }
    if (g_cfg.password[0]) {
        if (pwl != strlen(g_cfg.password) || memcmp(p + off, g_cfg.password, pwl)) {
            send_reject(c, "Password salah."); c->dead = 1;
            log_msg("Handshake gagal (password salah): %s", name);
            return;
        }
    }

    if (client_by_name(name)) { send_reject(c, "Nickname sudah dipakai."); c->dead = 1; return; }
    if (client_count() >= g_cfg.maxplayers) { send_reject(c, "Server penuh."); c->dead = 1; return; }
    int id = alloc_id();
    if (id < 0) { send_reject(c, "Server error."); c->dead = 1; return; }

    c->id = id;
    strcpy(c->name, name);
    c->handshaked = 1;
    c->interior = 0;
    c->vw = 0;
    /* Anti-cheat: nilai awal otoritatif server (lihat romp_anticheat.h). */
    c->ac_health = 100.0f;
    c->ac_armour = 0.0f;
    c->ac_set_ms = 0;
    c->ac_warnings = 0;
    c->ac_stat_known = 0;
    c->last_activity = now_ms();
    do_join(c);
}

static void handle_pos(Client *c, const uint8_t *p, size_t n) {
    if (!c->handshaked || n < 19) return;
    uint64_t t = now_ms();
    if (t - c->last_pos_ms < 30) return; /* rate limit 33 Hz */
    /* Simpan posisi dunia (format paket 0x10: float x,y,z,heading; u8 state;
     * u16 vehicle_model) agar bisa dipakai scripting (checkpoint, dsb). */
    float nx, ny, nz;
    memcpy(&nx, p, 4); memcpy(&ny, p + 4, 4); memcpy(&nz, p + 8, 4);
    /* Anti-cheat: cek teleport/speed sebelum last_pos_ms & posisi diperbarui.
     * Return 1 = teleport, paket dibuang (posisi curang tak disimpan). */
    if (ac_check_movement(c, nx, ny, nz, t)) return;
    c->last_pos_ms = t;
    c->pos_x = nx; c->pos_y = ny; c->pos_z = nz;
    c->pos_known = 1;
    uint8_t b[24];
    b[0] = P_PPOS;
    put_u16(b + 1, (uint16_t)c->id);
    memcpy(b + 3, p, 19);
    broadcast_scoped(b, 22, c, c); /* hanya ke yang se-vw & se-interior */
}

/* Laporan health/armour dari client (paket 0x26): dipakai anti-cheat.
 * Client yang belum mengimplementasikan 0x26 cukup tidak mengirimnya. */
static void handle_stat(Client *c, const uint8_t *p, size_t n) {
    if (!c->handshaked || n < 4) return;
    uint64_t t = now_ms();
    uint16_t hp = get_u16(p), ap = get_u16(p + 2);
    ac_check_stat(c, hp, ap, t);
}

static void handle_chat_cmd(Client *c, const char *msg);

static void handle_chat(Client *c, const uint8_t *p, size_t n) {
    if (!c->handshaked || n < 1) return;
    uint8_t ml = p[0];
    if (ml == 0 || ml > 128 || n < (size_t)(1 + ml)) return;

    /* anti-spam: maks 3 pesan per 2 detik */
    uint64_t t = now_ms();
    int recent = 0;
    for (int i = 0; i < 3; i++)
        if (t - c->chat_times[i] < 2000) recent++;
    if (recent >= 3) {
        chat_to(c, CHAT_SERVER, KIND_INFO, "*** Pelan-pelan, jangan spam.");
        return;
    }
    c->chat_times[c->chat_idx++ % 3] = t;

    char msg[129];
    memcpy(msg, p + 1, ml); msg[ml] = '\0';

    if (msg[0] == '/') { handle_chat_cmd(c, msg); return; }

    /* event Lua dulu; kalau di-handle, lewati broadcast default */
    if (lua_on_player_chat(c->id, c->name, msg)) return;

    char full[256];
    snprintf(full, sizeof(full), "%s (%d): %s", c->name, c->id, msg);
    chat_broadcast_scoped(c, (uint16_t)c->id, KIND_CHAT, full);
}

static void handle_chat_cmd(Client *c, const char *msg) {
    char cmd[16];
    if (sscanf(msg, "/%15s", cmd) != 1) return;

    /* pisahkan args (teks setelah command) */
    const char *args = msg + 1 + strlen(cmd);
    while (*args == ' ') args++;

    /* event Lua dulu; kalau di-handle, lewati command default */
    if (lua_on_player_command(c->id, c->name, cmd, args)) return;

    if (!strcmp(cmd, "q")) {
        c->dead = 1; c->kick_reason = LEAVE_QUIT;
        return;
    }
    if (!strcmp(cmd, "help")) {
        chat_to(c, CHAT_SERVER, KIND_INFO, "*** Command: /me /pm /players /help /q /inv (Lua: /waktu /info /kick)");
        return;
    }
    if (!strcmp(cmd, "players")) {
        char buf[512], *o = buf;
        o += snprintf(o, sizeof(buf) - (o - buf), "*** Online (%d): ", client_count());
        for (Client *p = g_clients; p; p = p->next) {
            if (!p->handshaked) continue;
            o += snprintf(o, sizeof(buf) - (o - buf), "%s(%d)%s ", p->name, p->id,
                          p->is_npc ? "[NPC]" : "");
            if ((size_t)(o - buf) > sizeof(buf) - 32) break;
        }
        chat_to(c, CHAT_SERVER, KIND_INFO, buf);
        return;
    }
    if (!strcmp(cmd, "me")) {
        const char *arg = msg + 3;
        while (*arg == ' ') arg++;
        if (!*arg) { chat_to(c, CHAT_SERVER, KIND_INFO, "*** Pakai: /me <aksi>"); return; }
        char full[256];
        snprintf(full, sizeof(full), "* %s %s", c->name, arg);
        chat_broadcast_scoped(c, (uint16_t)c->id, KIND_ME, full);
        return;
    }
    if (!strcmp(cmd, "pm")) {
        int id; char text[129];
        if (sscanf(msg, "/pm %d %128[^\n]", &id, text) != 2) {
            chat_to(c, CHAT_SERVER, KIND_INFO, "*** Pakai: /pm <id> <pesan>");
            return;
        }
        Client *t = client_by_id(id);
        if (!t || t == c) { chat_to(c, CHAT_SERVER, KIND_INFO, "*** Pemain tidak ditemukan."); return; }
        char full[256];
        snprintf(full, sizeof(full), "PM dari %s (%d): %s", c->name, c->id, text);
        chat_to(t, (uint16_t)c->id, KIND_PM, full);
        snprintf(full, sizeof(full), "PM ke %s (%d): %s", t->name, t->id, text);
        chat_to(c, (uint16_t)c->id, KIND_PM, full);
        return;
    }
    chat_to(c, CHAT_SERVER, KIND_INFO, "*** Command tidak dikenal. Ketik /help");
}

static void handle_ping(Client *c, const uint8_t *p, size_t n) {
    if (!c->handshaked || n < 4) return;
    uint8_t b[5];
    b[0] = P_PONG;
    memcpy(b + 1, p, 4);
    send_frame(c, b, 5);
}

/* Balasan probe ping server (0x30 -> 0x31). Echo u32 harus cocok dengan
 * probe terakhir; tanpa probe aktif (atau tak cocok) paket diabaikan. */
static void handle_pong(Client *c, const uint8_t *p, size_t n) {
    if (!c->handshaked || n < 4 || c->ping_probe_ms == 0) return;
    uint32_t echo; memcpy(&echo, p, 4);
    if (echo != c->ping_probe_tick) return;
    uint64_t dt = now_ms() - c->ping_probe_ms;
    c->ping_probe_ms = 0;
    c->ping_ms = dt > 9999 ? 9999 : (int)dt;
}

/* Kirim probe ping (0x30) ke semua client; hasil RTT diukur di handle_pong. */
static void ping_probe_all(uint64_t now) {
    for (Client *c = g_clients; c; c = c->next) {
        if (!c->handshaked || c->dead || c->is_npc) continue;
        uint32_t tick = (uint32_t)(now & 0xffffffffu);
        uint8_t b[5];
        b[0] = P_PING;
        memcpy(b + 1, &tick, 4);
        if (send_frame(c, b, 5) < 0) { c->dead = 1; continue; }
        c->ping_probe_ms = now;
        c->ping_probe_tick = tick;
    }
}

static void handle_nui_callback(Client *c, const uint8_t *p, size_t n) {
    if (!c->handshaked || n < 3) return;
    uint8_t nl = p[0];
    if (nl == 0 || nl > 64 || n < (size_t)(1 + nl + 2)) return;
    char name[65];
    memcpy(name, p + 1, nl); name[nl] = '\0';
    uint16_t dl = get_u16(p + 1 + nl);
    if (dl > 4000 || n < (size_t)(1 + nl + 2 + dl)) return;
    char data[4001];
    memcpy(data, p + 1 + nl + 2, dl); data[dl] = '\0';
    lua_on_nui_callback(c->id, name, data);
}

static void dispatch(Client *c, uint8_t type, const uint8_t *p, size_t n) {
    c->last_activity = now_ms();
    switch (type) {
        case P_HELLO: handle_hello(c, p, n); break;
        case P_POS:   handle_pos(c, p, n); break;
        case P_STAT:  handle_stat(c, p, n); break;
        case P_CHAT:  handle_chat(c, p, n); break;
        case P_PING:  handle_ping(c, p, n); break;
        case P_PONG:  handle_pong(c, p, n); break;
        case P_NUI_CB: handle_nui_callback(c, p, n); break;
        case P_VEH:   vehicle_on_packet(c, p, n); break;
        case P_WPN:   weapon_on_packet(c, p, n); break;
        default: break;
    }
}

/* return -1 jika koneksi mati */
static int pump_client(Client *c) {
    uint8_t tmp[2048];
#ifdef _WIN32
    int r = recv(c->sock, (char*)tmp, sizeof(tmp), 0);
#else
    ssize_t r = recv(c->sock, tmp, sizeof(tmp), 0);
#endif
    if (r == 0) return -1;
    if (r == SOCKET_ERROR) {
        int e = SOCKERR;
#ifdef _WIN32
        if (e == WSAEWOULDBLOCK) return 0;
#else
        if (e == EAGAIN || e == EWOULDBLOCK || e == EINTR) return 0;
#endif
        return -1;
    }
    if (c->recv_len + (size_t)r > sizeof(c->recv)) return -1;
    memcpy(c->recv + c->recv_len, tmp, (size_t)r);
    c->recv_len += (size_t)r;

    /* parse frame */
    while (c->recv_len >= 3) {
        uint16_t len = get_u16(c->recv);
        if (len < 1 || len > ROMP_MAX_FRAME) return -1;
        if (c->recv_len < (size_t)(2 + len)) break;
        dispatch(c, c->recv[2], c->recv + 3, len - 1);
        if (c->dead) break;
        memmove(c->recv, c->recv + 2 + len, c->recv_len - 2 - len);
        c->recv_len -= 2 + len;
    }
    return 0;
}

/* ---------------- Console ---------------- */
static pthread_mutex_t g_cmd_mutex = PTHREAD_MUTEX_INITIALIZER;
static char g_cmd_queue[16][256];
static int g_cmd_head = 0, g_cmd_tail = 0;

static void console_enqueue(const char *line) {
    pthread_mutex_lock(&g_cmd_mutex);
    int nxt = (g_cmd_head + 1) % 16;
    if (nxt != g_cmd_tail) {
        strncpy(g_cmd_queue[g_cmd_head], line, 255);
        g_cmd_queue[g_cmd_head][255] = '\0';
        g_cmd_head = nxt;
    }
    pthread_mutex_unlock(&g_cmd_mutex);
}

static int console_dequeue(char *out) {
    int got = 0;
    pthread_mutex_lock(&g_cmd_mutex);
    if (g_cmd_tail != g_cmd_head) {
        strcpy(out, g_cmd_queue[g_cmd_tail]);
        g_cmd_tail = (g_cmd_tail + 1) % 16;
        got = 1;
    }
    pthread_mutex_unlock(&g_cmd_mutex);
    return got;
}

static void *console_thread(void *arg) {
    (void)arg;
    char line[256];
    while (g_running && fgets(line, sizeof(line), stdin)) {
        size_t l = strlen(line);
        while (l > 0 && (line[l-1] == '\n' || line[l-1] == '\r')) line[--l] = '\0';
        if (l > 0) console_enqueue(line);
    }
    return NULL;
}

/* ---------------- Console & RCON ---------------- */
/* Output satu baris command: ke stdout bila o==NULL (console lokal),
 * atau di-append (+ '\n') ke buffer bila dipakai RCON. */
typedef struct { char *buf; size_t cap, len; } OutBuf;

static void emit_line(OutBuf *o, const char *fmt, ...) {
    char tmp[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (!o || !o->buf) { printf("%s\n", tmp); fflush(stdout); return; }
    size_t tl = strlen(tmp);
    if (o->len + tl + 1 >= o->cap) return; /* buffer penuh: sisa dibuang */
    memcpy(o->buf + o->len, tmp, tl);
    o->len += tl;
    o->buf[o->len++] = '\n';
    o->buf[o->len] = '\0';
}

/* Jalankan satu baris command console. Dipakai console lokal & RCON
 * (RCON menangkap output lewat OutBuf). */
static void console_run(const char *line, OutBuf *o) {
    char cmd[16];
    if (sscanf(line, "%15s", cmd) != 1) return;
    /* Lua dulu: onConsoleCommand(cmd, args) -> true = di-handle gamemode */
    {
        const char *args = line + strlen(cmd);
        while (*args == ' ' || *args == '\t') args++;
        if (lua_on_console_command(cmd, args)) return;
    }
    if (!strcmp(cmd, "help")) {
        emit_line(o, "  help              - daftar command");
        emit_line(o, "  say <pesan>       - kirim pengumuman ke semua pemain");
        emit_line(o, "  players           - daftar pemain online");
        emit_line(o, "  vehicles          - daftar kendaraan aktif");
        emit_line(o, "  kick <id> [alasan]- kick pemain");
        emit_line(o, "  ban <ip>          - ban IP (tidak bisa join)");
        emit_line(o, "  unban <ip>        - cabut ban IP");
        emit_line(o, "  bans              - daftar IP yang di-ban");
        emit_line(o, "  loadfs <nama>     - load filterscript (filterscripts/<nama>.lua)");
        emit_line(o, "  unloadfs <nama>   - unload filterscript");
        emit_line(o, "  reloadfs <nama>   - reload filterscript");
        emit_line(o, "  filterscripts     - daftar filterscript yang aktif");
        emit_line(o, "  npc <nama>        - sambungkan NPC/bot (koneksi virtual)");
        emit_line(o, "  unnpc <id>        - putuskan NPC/bot");
        emit_line(o, "  stop              - matikan server");
    } else if (!strcmp(cmd, "players")) {
        emit_line(o, "Online (%d/%d, NPC: %d):", client_count(), g_cfg.maxplayers,
                  npc_count());
        for (Client *c = g_clients; c; c = c->next)
            if (c->handshaked)
                emit_line(o, "  [%d] %s%s", c->id, c->name,
                          c->is_npc ? " [NPC]" : "");
    } else if (!strcmp(cmd, "vehicles")) {
        emit_line(o, "Kendaraan (%d):", vehicle_count());
        for (Vehicle *v = vehicle_first(); v; v = vehicle_next(v))
            emit_line(o, "  [vid %d] model %u pos %.1f,%.1f,%.1f hp %u%s driver %d",
                   v->id, v->model, v->x, v->y, v->z, v->health,
                   v->destroyed ? " (HANCUR)" : "", v->driver);
    } else if (!strcmp(cmd, "say")) {
        const char *msg = line + 3;
        while (*msg == ' ') msg++;
        if (*msg) server_say(msg);
    } else if (!strcmp(cmd, "kick")) {
        int id; char reason[128] = "";
        if (sscanf(line, "kick %d %127[^\n]", &id, reason) < 1) {
            emit_line(o, "Pakai: kick <id> [alasan]"); return;
        }
        Client *t = client_by_id(id);
        if (!t) { emit_line(o, "Pemain id %d tidak ditemukan.", id); return; }
        char nm[25];
        strncpy(nm, t->name, sizeof(nm) - 1); nm[sizeof(nm) - 1] = '\0';
        server_kick(id, reason);
        emit_line(o, "*** %s (id %d) di-kick.", nm, id);
    } else if (!strcmp(cmd, "ban")) {
        char ip[48] = "";
        if (sscanf(line, "ban %47s", ip) != 1) {
            emit_line(o, "Pakai: ban <ip>"); return;
        }
        if (ban_check(ip)) { emit_line(o, "IP %s sudah di-ban.", ip); return; }
        ban_add(ip);
        emit_line(o, "*** IP %s di-ban.", ip);
    } else if (!strcmp(cmd, "unban")) {
        char ip[48] = "";
        if (sscanf(line, "unban %47s", ip) != 1) {
            emit_line(o, "Pakai: unban <ip>"); return;
        }
        if (!ban_check(ip)) { emit_line(o, "IP %s tidak ada di daftar ban.", ip); return; }
        ban_remove(ip);
        emit_line(o, "*** IP %s di-unban.", ip);
    } else if (!strcmp(cmd, "bans")) {
        emit_line(o, "Daftar ban IP:");
        int n = 0;
        char ip[48];
        for (int i = 0; ban_get(i, ip, sizeof(ip)); i++) {
            emit_line(o, "  %s", ip);
            n++;
        }
        if (!n) emit_line(o, "  (kosong)");
    } else if (!strcmp(cmd, "loadfs")) {
        char nm[64] = "";
        if (sscanf(line, "loadfs %63s", nm) != 1) {
            emit_line(o, "Pakai: loadfs <nama>"); return;
        }
        int r = fs_load(nm);
        if (r == 0) emit_line(o, "*** Filterscript '%s' dimuat.", nm);
        else if (r == 1) emit_line(o, "Filterscript '%s' sudah dimuat.", nm);
        else emit_line(o, "Gagal memuat '%s' (cek filterscripts/%s.lua).", nm, nm);
    } else if (!strcmp(cmd, "unloadfs")) {
        char nm[64] = "";
        if (sscanf(line, "unloadfs %63s", nm) != 1) {
            emit_line(o, "Pakai: unloadfs <nama>"); return;
        }
        if (fs_unload(nm) == 0) emit_line(o, "*** Filterscript '%s' di-unload.", nm);
        else emit_line(o, "Filterscript '%s' tidak sedang dimuat.", nm);
    } else if (!strcmp(cmd, "reloadfs")) {
        char nm[64] = "";
        if (sscanf(line, "reloadfs %63s", nm) != 1) {
            emit_line(o, "Pakai: reloadfs <nama>"); return;
        }
        int r = fs_reload(nm);
        if (r == 0) emit_line(o, "*** Filterscript '%s' di-reload.", nm);
        else if (r == 1) emit_line(o, "Filterscript '%s' sudah dimuat.", nm);
        else emit_line(o, "Gagal me-reload '%s' (cek filterscripts/%s.lua).", nm, nm);
    } else if (!strcmp(cmd, "filterscripts")) {
        emit_line(o, "Filterscript aktif (%d/%d):", fs_count(), MAX_FILTERSCRIPTS);
        for (int i = 0; i < fs_count(); i++)
            emit_line(o, "  %s", fs_get_name(i));
        if (!fs_count()) emit_line(o, "  (kosong)");
    } else if (!strcmp(cmd, "npc")) {
        const char *nm = line + 3;
        while (*nm == ' ') nm++;
        if (!*nm) { emit_line(o, "Pakai: npc <nama>"); return; }
        char name[25];
        strncpy(name, nm, sizeof(name) - 1); name[sizeof(name) - 1] = '\0';
        /* potong di spasi pertama (nama tak boleh berspasi ala SA-MP) */
        for (char *sp = name; *sp; sp++)
            if (*sp == ' ' || *sp == '\t') { *sp = '\0'; break; }
        int id = npc_connect(name);
        if (id < 0)
            emit_line(o, "Gagal spawn NPC '%s' (nama invalid/sudah dipakai/server penuh).", name);
        else
            emit_line(o, "*** NPC '%s' terhubung (id %d).", name, id);
    } else if (!strcmp(cmd, "unnpc")) {
        int id;
        if (sscanf(line, "unnpc %d", &id) != 1) {
            emit_line(o, "Pakai: unnpc <id>"); return;
        }
        Client *t = client_by_id(id);
        if (!t || !t->is_npc) {
            emit_line(o, "NPC id %d tidak ditemukan.", id); return;
        }
        npc_disconnect(id, LEAVE_QUIT);
        emit_line(o, "*** NPC id %d diputus.", id);
    } else if (!strcmp(cmd, "stop")) {
        emit_line(o, "*** Server dimatikan...");
        g_running = 0;
    } else {
        emit_line(o, "Command tidak dikenal: %s (ketik 'help')", cmd);
    }
}

static void console_cmd(const char *line) {
    console_run(line, NULL);
}

/* ---------------- RCON (remote console ala SA-MP) ---------------- */
/* UDP di port game. Request:  "ROMP" 0x05 pwlen(u8) pw cmdlen(u16 LE) cmd
 * Reply:    "ROMP" 0x06 status(u8) outlen(u16 LE) out
 * status 0 = OK, 1 = password salah / RCON mati.
 * Anti brute-force: maks 2 request/detik per IP; 5x password salah dalam
 * waktu singkat = IP diblokir 60 detik (request berikutnya diabaikan diam). */
#define RCON_TRACK 32
typedef struct {
    char ip[48];
    uint64_t last_ms;
    int fails;
    uint64_t blocked_until;
} RconTrack;
static RconTrack g_rcon_track[RCON_TRACK];

static void rcon_reply(SOCKET usock, const struct sockaddr_in *from,
                       socklen_t flen, uint8_t status, const char *out, size_t outlen) {
    uint8_t b[1400];
    size_t o = 0;
    memcpy(b, QUERY_MAGIC, QUERY_MAGIC_LEN); o += QUERY_MAGIC_LEN;
    b[o++] = R_RCON_REPLY;
    b[o++] = status;
    if (outlen > 1000) outlen = 1000; /* muat di satu datagram UDP */
    put_u16(b + o, (uint16_t)outlen); o += 2;
    if (outlen) memcpy(b + o, out, outlen);
    o += outlen;
    sendto(usock, (char*)b, (int)o, 0, (struct sockaddr*)from, flen);
}

static void handle_rcon(SOCKET usock, const uint8_t *buf, size_t n,
                        const struct sockaddr_in *from, socklen_t flen) {
    char ip[48];
    inet_ntop(AF_INET, &from->sin_addr, ip, sizeof(ip));
    uint64_t now = now_ms();

    /* cari slot tracking IP ini, atau slot paling lama tak dipakai */
    int ti = -1;
    for (int i = 0; i < RCON_TRACK; i++)
        if (g_rcon_track[i].ip[0] && !strcmp(g_rcon_track[i].ip, ip)) { ti = i; break; }
    if (ti < 0) {
        ti = 0;
        for (int i = 1; i < RCON_TRACK; i++)
            if (!g_rcon_track[i].ip[0] ||
                g_rcon_track[i].last_ms < g_rcon_track[ti].last_ms) ti = i;
        strncpy(g_rcon_track[ti].ip, ip, sizeof(g_rcon_track[ti].ip) - 1);
        g_rcon_track[ti].ip[sizeof(g_rcon_track[ti].ip) - 1] = '\0';
        g_rcon_track[ti].last_ms = 0;
        g_rcon_track[ti].fails = 0;
        g_rcon_track[ti].blocked_until = 0;
    }
    RconTrack *t = &g_rcon_track[ti];
    if (now < t->blocked_until) return;       /* sedang diblokir: diam */
    if (now - t->last_ms < 500) return;        /* rate limit 2/detik per IP */
    t->last_ms = now;

    /* parse: pwlen pw cmdlen cmd */
    size_t o = QUERY_MAGIC_LEN + 1;
    if (n < o + 1) return;
    uint8_t pwl = buf[o++];
    if (pwl > 63 || n < o + pwl + 2) return;
    char pw[64];
    memcpy(pw, buf + o, pwl); pw[pwl] = '\0'; o += pwl;
    uint16_t cl = get_u16(buf + o); o += 2;
    if (cl == 0 || cl > 1023 || n < o + cl) return;
    char cmd[1024];
    memcpy(cmd, buf + o, cl); cmd[cl] = '\0';

    int enabled = g_cfg.rcon && g_cfg.rcon_password[0];
    int ok = enabled && pwl == strlen(g_cfg.rcon_password) &&
             !memcmp(pw, g_cfg.rcon_password, pwl);
    if (!ok) {
        t->fails++;
        if (t->fails >= 5) {
            t->blocked_until = now + 60000;
            t->fails = 0;
            log_msg("[rcon] brute-force dari %s: IP diblokir 60 dtk.", ip);
            return;
        }
        const char *msg = !enabled
            ? "RCON mati (set rcon_password di server.cfg)."
            : "Password RCON salah.";
        log_msg("[rcon] auth gagal dari %s.", ip);
        rcon_reply(usock, from, flen, 1, msg, strlen(msg));
        return;
    }
    t->fails = 0;

    log_msg("[rcon] %s: %s", ip, cmd);
    char obuf[1024];
    obuf[0] = '\0';
    OutBuf ob = { obuf, sizeof(obuf), 0 };
    console_run(cmd, &ob);
    rcon_reply(usock, from, flen, 0, obuf, ob.len);
}

/* ---------------- UDP query (server browser) ---------------- */
/* Format: "ROMP" + type(u8). Jawaban: "ROMP" + reply_type(u8) + payload. */
static void handle_query(SOCKET usock) {
    uint8_t buf[2048];
    struct sockaddr_in from;
    socklen_t flen = sizeof(from);
    ssize_t n = recvfrom(usock, (char*)buf, sizeof(buf), 0,
                         (struct sockaddr*)&from, &flen);
    if (n < (ssize_t)(QUERY_MAGIC_LEN + 1)) return;
    if (memcmp(buf, QUERY_MAGIC, QUERY_MAGIC_LEN)) return;
    uint8_t q = buf[QUERY_MAGIC_LEN];

    /* RCON jalan di socket UDP yang sama (butuh socket-nya ada; lihat main). */
    if (q == R_RCON) { handle_rcon(usock, buf, (size_t)n, &from, flen); return; }
    if (!g_cfg.query) return; /* query browser mati, tapi RCON tetap jalan */

    uint8_t out[1024];
    size_t o = 0;
    memcpy(out, QUERY_MAGIC, QUERY_MAGIC_LEN); o += QUERY_MAGIC_LEN;

    if (q == Q_INFO) {
        out[o++] = Q_INFO_REPLY;
        size_t hl = strlen(g_cfg.hostname), ml = strlen(g_cfg.motd);
        const char *ver = "romp1";
        size_t vl = strlen(ver);
        out[o++] = (uint8_t)hl; memcpy(out + o, g_cfg.hostname, hl); o += hl;
        out[o++] = (uint8_t)ml; memcpy(out + o, g_cfg.motd, ml); o += ml;
        out[o++] = (uint8_t)vl; memcpy(out + o, ver, vl); o += vl;
        put_u16(out + o, (uint16_t)client_count()); o += 2;
        put_u16(out + o, (uint16_t)g_cfg.maxplayers); o += 2;
        out[o++] = g_cfg.password[0] ? 1 : 0;
    } else if (q == Q_PLAYERS) {
        out[o++] = Q_PLAYERS_REPLY;
        uint8_t cnt = 0;
        size_t cntpos = o++;
        for (Client *c = g_clients; c && cnt < 100; c = c->next) {
            if (!c->handshaked) continue;
            size_t nl = strlen(c->name);
            if (o + 3 + nl > sizeof(out)) break;
            put_u16(out + o, (uint16_t)c->id); o += 2;
            out[o++] = (uint8_t)nl; memcpy(out + o, c->name, nl); o += nl;
            cnt++;
        }
        out[cntpos] = cnt;
    } else {
        return;
    }
    sendto(usock, (char*)out, (int)o, 0, (struct sockaddr*)&from, flen);
}

/* ---------------- Announce ke master server ---------------- */
static volatile int g_announce_now = 0;

void trigger_announce(void) { g_announce_now = 1; }

/* TCP connect dengan timeout (dipakai announce thread). -1 = gagal. */
static SOCKET tcp_connect_timeout(const char *host, int port, int timeout_ms) {
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t)port);
    /* coba sebagai IP dulu, lalu DNS */
    if (inet_pton(AF_INET, host, &a.sin_addr) != 1) {
        struct hostent *he = gethostbyname(host);
        if (!he) return INVALID_SOCKET;
        memcpy(&a.sin_addr, he->h_addr, 4);
    }
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) return INVALID_SOCKET;
    sock_nonblock(s);
    int rc = connect(s, (struct sockaddr*)&a, sizeof(a));
    if (rc == SOCKET_ERROR) {
#ifdef _WIN32
        int e = WSAGetLastError();
        if (e != WSAEWOULDBLOCK) { CLOSESOCK(s); return INVALID_SOCKET; }
#else
        if (errno != EINPROGRESS) { CLOSESOCK(s); return INVALID_SOCKET; }
#endif
        fd_set wf; FD_ZERO(&wf); FD_SET(s, &wf);
        struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
        if (select((int)(s + 1), NULL, &wf, NULL, &tv) <= 0) {
            CLOSESOCK(s); return INVALID_SOCKET;
        }
        int err = 0; socklen_t el = sizeof(err);
        getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&err, &el);
        if (err != 0) { CLOSESOCK(s); return INVALID_SOCKET; }
    }
    return s;
}

static int send_all_blocking(SOCKET s, const uint8_t *b, size_t n) {
    while (n > 0) {
        ssize_t r = send(s, (const char*)b, (int)n, 0);
        if (r <= 0) return -1;
        b += r; n -= (size_t)r;
    }
    return 0;
}

static void do_announce(void) {
    SOCKET s = tcp_connect_timeout(g_cfg.master_host, g_cfg.master_port, 5000);
    if (s == INVALID_SOCKET) {
        log_msg("[announce] master %s:%d tidak terjangkau.",
                g_cfg.master_host, g_cfg.master_port);
        return;
    }
    uint8_t p[512], f[516];
    size_t o = 0;
    size_t hl = strlen(g_cfg.hostname), ml = strlen(g_cfg.motd);
    if (hl > 63) hl = 63; if (ml > 255) ml = 255;
    put_u16(p + o, (uint16_t)g_cfg.port); o += 2;
    put_u16(p + o, (uint16_t)client_count()); o += 2;
    put_u16(p + o, (uint16_t)g_cfg.maxplayers); o += 2;
    p[o++] = (uint8_t)hl; memcpy(p + o, g_cfg.hostname, hl); o += hl;
    p[o++] = (uint8_t)ml; memcpy(p + o, g_cfg.motd, ml); o += ml;
    put_u16(f, (uint16_t)(o + 1)); f[2] = M_ANNOUNCE; memcpy(f + 3, p, o);
    if (send_all_blocking(s, f, o + 3) == 0)
        log_msg("[announce] terdaftar di master %s:%d.",
                g_cfg.master_host, g_cfg.master_port);
    CLOSESOCK(s);
}

static void *announce_thread(void *arg) {
    (void)arg;
    /* announce pertama langsung */
    if (g_cfg.announce && g_cfg.master_host[0]) do_announce();
    while (g_running) {
        for (int i = 0; i < 60 && g_running; i++) {
            if (g_announce_now) { g_announce_now = 0; break; }
#ifdef _WIN32
            Sleep(1000);
#else
            sleep(1);
#endif
        }
        if (!g_running) break;
        if (g_cfg.announce && g_cfg.master_host[0]) do_announce();
    }
    return NULL;
}

/* ---------------- LAN discovery beacon ---------------- */
/* Broadcast tiap 5 detik ke 255.255.255.255:7779 supaya client di LAN
 * yang sama otomatis menemukan server tanpa master/VPS. */
static SOCKET g_beacon_sock = INVALID_SOCKET;

static void beacon_init(void) {
    if (!g_cfg.lan_discovery) return;
    g_beacon_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_beacon_sock == INVALID_SOCKET) {
        log_msg("WARNING: socket beacon LAN gagal.");
        return;
    }
    int one = 1;
    setsockopt(g_beacon_sock, SOL_SOCKET, SO_BROADCAST, (char*)&one, sizeof(one));
}

static void beacon_send(void) {
    if (g_beacon_sock == INVALID_SOCKET) return;
    uint8_t out[512];
    size_t o = 0;
    memcpy(out, QUERY_MAGIC, QUERY_MAGIC_LEN); o += QUERY_MAGIC_LEN;
    out[o++] = LAN_BEACON;
    put_u16(out + o, (uint16_t)g_cfg.port); o += 2;    size_t hl = strlen(g_cfg.hostname), ml = strlen(g_cfg.motd);
    if (hl > 63) hl = 63; if (ml > 200) ml = 200;
    out[o++] = (uint8_t)hl; memcpy(out + o, g_cfg.hostname, hl); o += hl;
    put_u16(out + o, (uint16_t)client_count()); o += 2;
    put_u16(out + o, (uint16_t)g_cfg.maxplayers); o += 2;
    out[o++] = g_cfg.password[0] ? 1 : 0;
    out[o++] = (uint8_t)ml; memcpy(out + o, g_cfg.motd, ml); o += ml;

    struct sockaddr_in bc;
    memset(&bc, 0, sizeof(bc));
    bc.sin_family = AF_INET;
    bc.sin_port = htons(LAN_BEACON_PORT);
    bc.sin_addr.s_addr = INADDR_BROADCAST;
    sendto(g_beacon_sock, (char*)out, (int)o, 0,
           (struct sockaddr*)&bc, sizeof(bc));
}

static void beacon_close(void) {
    if (g_beacon_sock != INVALID_SOCKET) {
        CLOSESOCK(g_beacon_sock);
        g_beacon_sock = INVALID_SOCKET;
    }
}

/* ---------------- Accept & cleanup ---------------- */
static void accept_new(SOCKET lsock) {
    struct sockaddr_in addr;
    socklen_t alen = sizeof(addr);
    SOCKET s = accept(lsock, (struct sockaddr*)&addr, &alen);
    if (s == INVALID_SOCKET) return;
    sock_nonblock(s);

    Client *c = (Client*)calloc(1, sizeof(Client));
    c->sock = s;
    c->id = -1;
    c->vehicle_id = -1;
    c->vehicle_seat = -1;
    c->ping_ms = -1; /* belum terukur sampai probe pertama dijawab */
    c->last_activity = now_ms();
    c->next = g_clients;
    g_clients = c;
    char ip[48];
    inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
    strncpy(c->ip, ip, sizeof(c->ip) - 1);
    log_msg("Koneksi baru dari %s", ip);
}

static void sweep_dead(void) {
    Client **pp = &g_clients;
    while (*pp) {
        Client *c = *pp;
        int gone = c->dead;
        /* NPC tak punya socket & tak kirim paket: jangan timeout. */
        if (!gone && !c->is_npc && c->handshaked && now_ms() - c->last_activity > 90000) {
            log_msg("Timeout: %s (id %d)", c->name, c->id);
            c->kick_reason = LEAVE_TIMEOUT;
            gone = 1;
        }
        if (gone) {
            /* Keluarkan diam-diam dari kendaraan sebelum event leave. */
            vehicle_remove_player_silent(c->id);
            do_leave(c, c->kick_reason);
            CLOSESOCK(c->sock);
            *pp = c->next;
            free(c);
        } else {
            pp = &c->next;
        }
    }
}

/* ---------------- Main ---------------- */
#ifndef _WIN32
static void on_signal(int sig) { (void)sig; g_running = 0; }
#else
static BOOL WINAPI on_ctrl(DWORD t) {
    if (t == CTRL_C_EVENT || t == CTRL_CLOSE_EVENT) { g_running = 0; return TRUE; }
    return FALSE;
}
#endif

int main(int argc, char **argv) {
    const char *cfg_path = "server.cfg";
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-c") && i + 1 < argc) cfg_path = argv[++i];
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            printf("Pakai: romp-server [-c server.cfg]\n");
            return 0;
        }
    }
    cfg_load(cfg_path);

#ifdef _WIN32
    WSADATA wd;
    if (WSAStartup(MAKEWORD(2, 2), &wd) != 0) {
        fprintf(stderr, "WSAStartup gagal\n");
        return 1;
    }
    SetConsoleCtrlHandler(on_ctrl, TRUE);
#else
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
#endif

    g_logf = fopen("romp-server.log", "a");

    SOCKET lsock = socket(AF_INET, SOCK_STREAM, 0);
    if (lsock == INVALID_SOCKET) { fprintf(stderr, "socket gagal\n"); return 1; }
    int one = 1;
    setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, sizeof(one));
    sock_nonblock(lsock);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)g_cfg.port);
    if (bind(lsock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        fprintf(stderr, "bind port %d gagal\n", g_cfg.port);
        return 1;
    }
    if (listen(lsock, 16) == SOCKET_ERROR) { fprintf(stderr, "listen gagal\n"); return 1; }

    /* socket UDP untuk server browser query + RCON (port yang sama) */
    SOCKET usock = INVALID_SOCKET;
    int need_udp = g_cfg.query || (g_cfg.rcon && g_cfg.rcon_password[0]);
    if (need_udp) {
        usock = socket(AF_INET, SOCK_DGRAM, 0);
        if (usock != INVALID_SOCKET) {
            struct sockaddr_in uaddr;
            memset(&uaddr, 0, sizeof(uaddr));
            uaddr.sin_family = AF_INET;
            uaddr.sin_addr.s_addr = INADDR_ANY;
            uaddr.sin_port = htons((uint16_t)g_cfg.port);
            if (bind(usock, (struct sockaddr*)&uaddr, sizeof(uaddr)) == SOCKET_ERROR) {
                CLOSESOCK(usock); usock = INVALID_SOCKET;
                log_msg("WARNING: bind UDP query gagal, server browser query & RCON mati.");
            } else {
                sock_nonblock(usock);
            }
        }
    }

    log_msg("===========================================");
    log_msg(" RO-MP Dedicated Server v1 (Lua gamemode)");
    log_msg(" Hostname : %s", g_cfg.hostname);
    log_msg(" Port     : %d", g_cfg.port);
    log_msg(" Max pemain: %d%s", g_cfg.maxplayers, g_cfg.password[0] ? " (password ON)" : "");
    log_msg(" MOTD     : %s", g_cfg.motd);
    log_msg(" Gamemode : %s", g_cfg.gamemode);
    if (g_cfg.announce && g_cfg.master_host[0])
        log_msg(" Announce : ke master %s:%d tiap 60 dtk",
                g_cfg.master_host, g_cfg.master_port);
    if (g_cfg.query) log_msg(" UDP query: aktif (server browser)");
    if (g_cfg.rcon && g_cfg.rcon_password[0])
        log_msg(" RCON     : aktif via UDP (rcon_password diset)");
    else
        log_msg(" RCON     : mati (set rcon_password di server.cfg untuk mengaktifkan)");
    if (g_cfg.lan_discovery) log_msg(" LAN beacon: broadcast tiap %d dtk (port %d)",
                                     LAN_BEACON_MS / 1000, LAN_BEACON_PORT);
    log_msg(" Ketik 'help' untuk command console.");
    log_msg("===========================================");

    /* Database SQLite opsional untuk gamemode (skor, akun).
     * Dibuka SEBELUM lua_init agar romp.db sudah tersedia di onGamemodeInit. */
    if (g_cfg.database[0]) {
        int dbh = db_open(g_cfg.database);
        if (dbh >= 0) {
            db_set_default(dbh);
            log_msg(" Database : %s (akses Lua via romp.db)", g_cfg.database);
        } else {
            log_msg(" WARNING: database '%s' gagal dibuka: %s",
                    g_cfg.database, db_last_error(-1));
        }
    }

    if (lua_init(g_cfg.gamemode) != 0)
        log_msg("WARNING: jalan tanpa gamemode Lua (pakai default C).");
    else
        fs_load_configured(g_cfg.filterscripts); /* ala SA-MP: filterscripts a b c */

    pthread_t cthread;
    pthread_create(&cthread, NULL, console_thread, NULL);

    pthread_t athr = 0;
    if (g_cfg.announce && g_cfg.master_host[0])
        pthread_create(&athr, NULL, announce_thread, NULL);

    beacon_init();

    uint64_t last_lua_tick = now_ms();
    uint64_t last_beacon_ms = 0;
    uint64_t last_ping_ms = 0;
    while (g_running) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(lsock, &rfds);
        SOCKET mx = lsock;
        if (usock != INVALID_SOCKET) {
            FD_SET(usock, &rfds);
            if (usock > mx) mx = usock;
        }
        for (Client *c = g_clients; c; c = c->next) {
            if (c->is_npc) continue; /* NPC tak punya socket: jangan FD_SET(-1) */
            FD_SET(c->sock, &rfds);
            if (c->sock > mx) mx = c->sock;
        }
        struct timeval tv = { 0, 100000 }; /* 100 ms */
        int r = select((int)(mx + 1), &rfds, NULL, NULL, &tv);
        if (r == SOCKET_ERROR) {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEINTR) continue;
#else
            if (errno == EINTR) continue;
#endif
            break;
        }
        if (r > 0) {
            if (FD_ISSET(lsock, &rfds)) accept_new(lsock);
            if (usock != INVALID_SOCKET && FD_ISSET(usock, &rfds)) handle_query(usock);
            for (Client *c = g_clients; c; c = c->next) {
                if (c->dead || c->is_npc) continue; /* NPC tak pernah di-pump */
                if (FD_ISSET(c->sock, &rfds)) {
                    if (pump_client(c) < 0) { c->dead = 1; c->kick_reason = LEAVE_QUIT; }
                }
            }
        }

        char line[256];
        while (console_dequeue(line)) console_cmd(line);

        /* tick Lua tiap 1 detik */
        uint64_t tnow = now_ms();
        if (tnow - last_lua_tick >= 1000) {
            last_lua_tick = tnow;
            lua_on_tick();
            vehicle_tick(tnow); /* respawn kendaraan hancur */
        }

        /* timer Lua ala SA-MP SetTimer (tiap iterasi loop, ~100 ms) */
        timer_tick(tnow);

        /* beacon LAN tiap LAN_BEACON_MS */
        if (g_cfg.lan_discovery && tnow - last_beacon_ms >= LAN_BEACON_MS) {
            last_beacon_ms = tnow;
            beacon_send();
        }

        /* probe ping ala SA-MP tiap 5 detik (0x30 -> 0x31, RTT di handle_pong) */
        if (tnow - last_ping_ms >= 5000) {
            last_ping_ms = tnow;
            ping_probe_all(tnow);
        }

        sweep_dead();
    }

    log_msg("Server dimatikan...");
    g_running = 0;
    /* Bangunkan console thread yang mungkin ke-block di fgets(stdin).
     * Tanpa ini, exit() deadlock: thread itu megang stdio lock selamanya. */
    pthread_cancel(cthread);
    pthread_join(cthread, NULL);
    if (athr) pthread_join(athr, NULL);
    lua_shutdown();
    db_close_all(); /* tutup database SQLite gamemode */
    ban_clear();
    beacon_close();
    for (Client *c = g_clients; c; c = c->next) CLOSESOCK(c->sock);
    CLOSESOCK(lsock);
    if (usock != INVALID_SOCKET) CLOSESOCK(usock);
    if (g_logf) fclose(g_logf);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
