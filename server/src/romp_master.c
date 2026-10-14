/*
 * RO-MP Master Server — daftar server untuk server browser.
 *
 * Cara pakai:
 *   ./romp-master [-p port]        (default port 7778)
 *
 * Game server mendaftarkan diri dengan announce periodik (lihat opsi
 * `announce`/`master_host`/`master_port` di server.cfg). Client mengambil
 * daftar via M_LIST_REQ lalu meng-query tiap server via UDP untuk info live.
 *
 * Protokol TCP (framing sama dengan game server: u16 LE length + type):
 *   Server -> Master  0x50 ANNOUNCE:
 *       gameport(u16) players(u16) maxplayers(u16)
 *       hostnamelen(u8) hostname motdlen(u8) motd
 *     -> IP server diambil dari koneksi (getpeername).
 *   Client -> Master  0x51 LIST_REQUEST (kosong)
 *   Master -> Client  0x52 LIST:
 *       count(u16) + per server:
 *       iplen(u8) ip port(u16) players(u16) maxplayers(u16)
 *       hostnamelen(u8) hostname motdlen(u8) motd
 *
 * Entri yang tidak di-announce ulang dalam 180 detik dianggap mati
 * dan dihapus otomatis.
 *
 * Build: lihat Makefile (target `master`).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <time.h>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
   typedef SOCKET sock_t;
#  define CLOSESOCK(s) closesocket(s)
#  define INVALID_SOCK INVALID_SOCKET
#  define SLEEP_MS(ms) Sleep(ms)
#else
#  include <unistd.h>
#  include <errno.h>
#  include <fcntl.h>
#  include <pthread.h>
#  include <netdb.h>
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <sys/select.h>
   typedef int sock_t;
#  define CLOSESOCK(s) close(s)
#  define INVALID_SOCK (-1)
#  define SOCKET_ERROR (-1)
#  define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

#define M_ANNOUNCE  0x50
#define M_LIST_REQ  0x51
#define M_LIST      0x52
#define ENTRY_TTL_S 180
#define MAX_SERVERS 512

typedef struct {
    char     ip[48];
    uint16_t port;
    uint16_t players;
    uint16_t maxplayers;
    char     hostname[64];
    char     motd[256];
    time_t   last_seen;
    int      used;
} Entry;

static Entry g_entries[MAX_SERVERS];
static volatile int g_running = 1;

static uint16_t get_u16(const uint8_t *b) { return (uint16_t)(b[0] | (b[1] << 8)); }
static void put_u16(uint8_t *b, uint16_t v) { b[0] = v & 0xFF; b[1] = (v >> 8) & 0xFF; }

static void logm(const char *fmt, ...) {
    char ts[32]; time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));
    printf("[%s] ", ts);
    va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
    printf("\n"); fflush(stdout);
}

static int send_all(sock_t s, const uint8_t *b, size_t n) {
    while (n > 0) {
        int r = send(s, (const char*)b, (int)n, 0);
        if (r <= 0) return -1;
        b += r; n -= (size_t)r;
    }
    return 0;
}

/* Cari entri by ip+port, atau slot kosong. */
static Entry *find_entry(const char *ip, uint16_t port) {
    Entry *free_slot = NULL;
    for (int i = 0; i < MAX_SERVERS; i++) {
        if (g_entries[i].used && !strcmp(g_entries[i].ip, ip) &&
            g_entries[i].port == port)
            return &g_entries[i];
        if (!g_entries[i].used && !free_slot) free_slot = &g_entries[i];
    }
    return free_slot;
}

static void handle_announce(const uint8_t *p, size_t n, const char *ip) {
    if (n < 7) return;
    uint16_t gameport = get_u16(p);
    uint16_t players = get_u16(p + 2);
    uint16_t maxplayers = get_u16(p + 4);
    size_t o = 6;
    if (o + 1 > n) return;
    uint8_t hl = p[o++];
    if (hl > 63 || o + hl + 1 > n) return;
    char hostname[64]; memcpy(hostname, p + o, hl); hostname[hl] = 0; o += hl;
    uint8_t ml = p[o++];
    if (o + ml > n) return;
    char motd[256]; memcpy(motd, p + o, ml); motd[ml] = 0;

    Entry *e = find_entry(ip, gameport);
    if (!e) return; /* penuh */
    int baru = !e->used;
    e->used = 1;
    strncpy(e->ip, ip, sizeof(e->ip) - 1);
    e->port = gameport; e->players = players; e->maxplayers = maxplayers;
    strcpy(e->hostname, hostname); strcpy(e->motd, motd);
    e->last_seen = time(NULL);
    logm("%s %s:%u \"%s\" (%u/%u)", baru ? "ANNOUNCE baru:" : "ANNOUNCE update:",
         ip, gameport, hostname, players, maxplayers);
}

static void send_list(sock_t s) {
    uint8_t out[16384];
    size_t o = 3; /* 2 byte len + 1 byte type diisi belakangan */
    uint16_t cnt = 0;
    size_t cntpos = o; o += 2;
    time_t now = time(NULL);
    for (int i = 0; i < MAX_SERVERS && o < sizeof(out) - 400; i++) {
        Entry *e = &g_entries[i];
        if (!e->used) continue;
        if (now - e->last_seen > ENTRY_TTL_S) continue;
        size_t il = strlen(e->ip), hl = strlen(e->hostname), ml = strlen(e->motd);
        out[o++] = (uint8_t)il; memcpy(out + o, e->ip, il); o += il;
        put_u16(out + o, e->port); o += 2;
        put_u16(out + o, e->players); o += 2;
        put_u16(out + o, e->maxplayers); o += 2;
        out[o++] = (uint8_t)hl; memcpy(out + o, e->hostname, hl); o += hl;
        out[o++] = (uint8_t)ml; memcpy(out + o, e->motd, ml); o += ml;
        cnt++;
    }
    put_u16(out + cntpos, cnt);
    put_u16(out, (uint16_t)(o - 2));
    out[2] = M_LIST;
    send_all(s, out, o);
}

static void sweep_expired(void) {
    time_t now = time(NULL);
    for (int i = 0; i < MAX_SERVERS; i++) {
        if (g_entries[i].used && now - g_entries[i].last_seen > ENTRY_TTL_S) {
            logm("EXPIRE %s:%u \"%s\"", g_entries[i].ip, g_entries[i].port,
                 g_entries[i].hostname);
            g_entries[i].used = 0;
        }
    }
}

static int entry_count(void) {
    int c = 0;
    for (int i = 0; i < MAX_SERVERS; i++) if (g_entries[i].used) c++;
    return c;
}

#ifdef _WIN32
static DWORD WINAPI console_thread(LPVOID arg) {
#else
static void *console_thread(void *arg) {
#endif
    (void)arg;
    char line[128];
    while (g_running && fgets(line, sizeof(line), stdin)) {
        if (!strncmp(line, "list", 4)) {
            printf("Server terdaftar (%d):\n", entry_count());
            for (int i = 0; i < MAX_SERVERS; i++) {
                Entry *e = &g_entries[i];
                if (!e->used) continue;
                printf("  %s:%u \"%s\" %u/%u\n", e->ip, e->port,
                       e->hostname, e->players, e->maxplayers);
            }
            fflush(stdout);
        } else if (!strncmp(line, "stop", 4)) {
            g_running = 0; break;
        } else if (!strncmp(line, "help", 4)) {
            printf("  list - daftar server\n  stop - matikan master\n");
            fflush(stdout);
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    int port = 7778;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-p") && i + 1 < argc) port = atoi(argv[++i]);
        else { printf("Pakai: %s [-p port]\n", argv[0]); return 1; }
    }
#ifdef _WIN32
    WSADATA wd; WSAStartup(MAKEWORD(2, 2), &wd);
#endif
    sock_t ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls == INVALID_SOCK) { fprintf(stderr, "socket gagal\n"); return 1; }
    int one = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (char*)&one, sizeof(one));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET; a.sin_addr.s_addr = INADDR_ANY;
    a.sin_port = htons((uint16_t)port);
    if (bind(ls, (struct sockaddr*)&a, sizeof(a)) == SOCKET_ERROR ||
        listen(ls, 16) == SOCKET_ERROR) {
        fprintf(stderr, "bind/listen port %d gagal\n", port); return 1;
    }
    logm("===========================================");
    logm(" RO-MP Master Server (server browser)");
    logm(" Port: %d | TTL entri: %d dtk", port, ENTRY_TTL_S);
    logm("===========================================");

#ifdef _WIN32
    CreateThread(NULL, 0, console_thread, NULL, 0, NULL);
#else
    pthread_t ct; pthread_create(&ct, NULL, console_thread, NULL);
#endif

    uint8_t rbuf[2048];
    while (g_running) {
        fd_set rf; FD_ZERO(&rf); FD_SET(ls, &rf);
        struct timeval tv = { 1, 0 };
        int r = select((int)(ls + 1), &rf, NULL, NULL, &tv);
        if (r > 0 && FD_ISSET(ls, &rf)) {
            struct sockaddr_in ca; socklen_t cl = sizeof(ca);
            sock_t s = accept(ls, (struct sockaddr*)&ca, &cl);
            if (s != INVALID_SOCK) {
                char ip[48];
                inet_ntop(AF_INET, &ca.sin_addr, ip, sizeof(ip));
                /* baca 1 frame (blocking singkat, client lokal/master kecil) */
#ifdef _WIN32
                DWORD to = 3000; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&to, sizeof(to));
#else
                struct timeval to = { 3, 0 };
                setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));
#endif
                size_t got = 0;
                while (got < 2) {
                    int k = recv(s, (char*)rbuf + got, 2 - (int)got, 0);
                    if (k <= 0) break;
                    got += (size_t)k;
                }
                if (got == 2) {
                    uint16_t ln = get_u16(rbuf);
                    if (ln >= 1 && ln <= sizeof(rbuf) - 3) {
                        size_t need = ln; got = 0;
                        while (got < need) {
                            int k = recv(s, (char*)rbuf + 2 + got,
                                         (int)(need - got), 0);
                            if (k <= 0) break;
                            got += (size_t)k;
                        }
                        if (got == need) {
                            uint8_t type = rbuf[2];
                            if (type == M_ANNOUNCE)
                                handle_announce(rbuf + 3, ln - 1, ip);
                            else if (type == M_LIST_REQ)
                                send_list(s);
                        }
                    }
                }
                CLOSESOCK(s);
            }
        }
        sweep_expired();
    }
    logm("Master dimatikan.");
    CLOSESOCK(ls);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
