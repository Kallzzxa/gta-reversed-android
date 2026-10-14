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
    "gamemode.lua"
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
    }
    fclose(f);
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
static int g_running = 1;
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
    uint8_t hdr[2];
    put_u16(hdr, (uint16_t)n);
    if (send_all(c->sock, hdr, 2) < 0) return -1;
    return send_all(c->sock, payload, n);
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
        if (!c->handshaked || c->dead || c == except) continue;
        if (send_frame(c, payload, n) < 0) c->dead = 1;
    }
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

void server_say(const char *msg) {
    char full[256];
    snprintf(full, sizeof(full), "*** %s", msg);
    chat_broadcast(CHAT_SERVER, KIND_INFO, full);
}

/* ---------------- Join / Leave ---------------- */
static void send_player_list(Client *c) {
    uint8_t b[4096];
    b[0] = P_LIST;
    int n = 0;
    size_t off = 2;
    for (Client *o = g_clients; o; o = o->next) {
        if (!o->handshaked || o == c || o->dead) continue;
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
    size_t nl = strlen(c->name);

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

    /* beritahu yang lain */
    b[0] = P_JOIN;
    put_u16(b + 1, (uint16_t)c->id);
    b[3] = (uint8_t)nl;
    memcpy(b + 4, c->name, nl);
    broadcast(b, 4 + nl, c);

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
    uint8_t b[4];
    b[0] = P_LEAVE;
    put_u16(b + 1, (uint16_t)c->id);
    b[3] = (uint8_t)reason;
    broadcast(b, 4, c);
    const char *rs = reason == LEAVE_KICK ? "di-kick" : reason == LEAVE_TIMEOUT ? "timeout" : "keluar";
    char msg[128];
    snprintf(msg, sizeof(msg), "*** %s (id %d) %s.", c->name, c->id, rs);
    if (!lua_on_player_leave(c->id, c->name, reason))
        chat_broadcast(CHAT_SERVER, KIND_SYS, msg);
    log_msg("%s keluar (%s). Online: %d", c->name, rs, client_count());
}

/* Kick pemain + umumkan. Dipakai console & Lua. */
void server_kick(int id, const char *reason) {
    Client *c = client_by_id(id);
    if (!c) return;
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
    c->last_activity = now_ms();
    do_join(c);
}

static void handle_pos(Client *c, const uint8_t *p, size_t n) {
    if (!c->handshaked || n < 19) return;
    uint64_t t = now_ms();
    if (t - c->last_pos_ms < 30) return; /* rate limit 33 Hz */
    c->last_pos_ms = t;
    uint8_t b[24];
    b[0] = P_PPOS;
    put_u16(b + 1, (uint16_t)c->id);
    memcpy(b + 3, p, 19);
    broadcast(b, 22, c);
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
    chat_broadcast((uint16_t)c->id, KIND_CHAT, full);
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
        chat_to(c, CHAT_SERVER, KIND_INFO, "*** Command: /me /pm /players /help /q");
        return;
    }
    if (!strcmp(cmd, "players")) {
        char buf[512], *o = buf;
        o += snprintf(o, sizeof(buf) - (o - buf), "*** Online (%d): ", client_count());
        for (Client *p = g_clients; p; p = p->next) {
            if (!p->handshaked) continue;
            o += snprintf(o, sizeof(buf) - (o - buf), "%s(%d) ", p->name, p->id);
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
        chat_broadcast((uint16_t)c->id, KIND_ME, full);
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

static void dispatch(Client *c, uint8_t type, const uint8_t *p, size_t n) {
    c->last_activity = now_ms();
    switch (type) {
        case P_HELLO: handle_hello(c, p, n); break;
        case P_POS:   handle_pos(c, p, n); break;
        case P_CHAT:  handle_chat(c, p, n); break;
        case P_PING:  handle_ping(c, p, n); break;
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

static void console_cmd(const char *line) {
    char cmd[16];
    if (sscanf(line, "%15s", cmd) != 1) return;
    if (!strcmp(cmd, "help")) {
        printf("  help              - daftar command\n");
        printf("  say <pesan>       - kirim pengumuman ke semua pemain\n");
        printf("  players           - daftar pemain online\n");
        printf("  kick <id> [alasan]- kick pemain\n");
        printf("  stop              - matikan server\n");
    } else if (!strcmp(cmd, "players")) {
        printf("Online (%d/%d):\n", client_count(), g_cfg.maxplayers);
        for (Client *c = g_clients; c; c = c->next)
            if (c->handshaked) printf("  [%d] %s\n", c->id, c->name);
    } else if (!strcmp(cmd, "say")) {
        const char *msg = line + 3;
        while (*msg == ' ') msg++;
        if (*msg) server_say(msg);
    } else if (!strcmp(cmd, "kick")) {
        int id; char reason[128] = "";
        if (sscanf(line, "kick %d %127[^\n]", &id, reason) < 1) {
            printf("Pakai: kick <id> [alasan]\n"); return;
        }
        if (!client_by_id(id)) { printf("Pemain id %d tidak ditemukan.\n", id); return; }
        server_kick(id, reason);
    } else if (!strcmp(cmd, "stop")) {
        g_running = 0;
    } else {
        printf("Command tidak dikenal: %s (ketik 'help')\n", cmd);
    }
    fflush(stdout);
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
    c->last_activity = now_ms();
    c->next = g_clients;
    g_clients = c;
    char ip[32];
    inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
    log_msg("Koneksi baru dari %s", ip);
}

static void sweep_dead(void) {
    Client **pp = &g_clients;
    while (*pp) {
        Client *c = *pp;
        int gone = c->dead;
        if (!gone && c->handshaked && now_ms() - c->last_activity > 90000) {
            log_msg("Timeout: %s (id %d)", c->name, c->id);
            c->kick_reason = LEAVE_TIMEOUT;
            gone = 1;
        }
        if (gone) {
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

    log_msg("===========================================");
    log_msg(" RO-MP Dedicated Server v1 (Lua gamemode)");
    log_msg(" Hostname : %s", g_cfg.hostname);
    log_msg(" Port     : %d", g_cfg.port);
    log_msg(" Max pemain: %d%s", g_cfg.maxplayers, g_cfg.password[0] ? " (password ON)" : "");
    log_msg(" MOTD     : %s", g_cfg.motd);
    log_msg(" Gamemode : %s", g_cfg.gamemode);
    log_msg(" Ketik 'help' untuk command console.");
    log_msg("===========================================");

    if (lua_init(g_cfg.gamemode) != 0)
        log_msg("WARNING: jalan tanpa gamemode Lua (pakai default C).");

    pthread_t cthread;
    pthread_create(&cthread, NULL, console_thread, NULL);

    uint64_t last_lua_tick = now_ms();
    while (g_running) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(lsock, &rfds);
        SOCKET mx = lsock;
        for (Client *c = g_clients; c; c = c->next) {
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
            for (Client *c = g_clients; c; c = c->next) {
                if (c->dead) continue;
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
        }

        sweep_dead();
    }

    log_msg("Server dimatikan...");
    lua_shutdown();
    for (Client *c = g_clients; c; c = c->next) CLOSESOCK(c->sock);
    CLOSESOCK(lsock);
    if (g_logf) fclose(g_logf);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
