/*
 * RO-MP Dedicated Server — header internal.
 * Dipakai bersama oleh romp_server.c (core network) dan romp_lua.c (scripting Lua).
 */
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef _WIN32
#define FD_SETSIZE 128
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef int socklen_t;
#define CLOSESOCK closesocket
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define CLOSESOCK close
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
typedef int SOCKET;
#endif

/* ---------------- Protokol (mirror dari PROTOCOL.md) ---------------- */
#define ROMP_PROTO_VER   1
#define ROMP_MAX_FRAME   4096

#define P_HELLO       0x01
#define P_WELCOME     0x02
#define P_REJECT      0x03
#define P_POS         0x10
#define P_PPOS        0x11
#define P_CHAT        0x20
#define P_CHAT_MSG    0x21
#define P_JOIN        0x22
#define P_LEAVE       0x23
#define P_LIST        0x24
#define P_PING        0x30
#define P_PONG        0x31
#define P_NUI_MSG     0x40
#define P_NUI_CB      0x41
#define P_NUI_FOCUS   0x42

/* ---- Protokol UDP query (server browser), port sama dengan TCP ---- */
#define QUERY_MAGIC   "ROMP"
#define QUERY_MAGIC_LEN 4
#define Q_INFO          0x01  /* client -> server: minta info */
#define Q_INFO_REPLY    0x02  /* server -> client: info server */
#define Q_PLAYERS       0x03  /* client -> server: minta daftar pemain */
#define Q_PLAYERS_REPLY 0x04  /* server -> client: daftar pemain */

/* ---- LAN discovery: server broadcast beacon tiap 3 detik ---- */
#define LAN_BEACON_PORT 7779  /* port UDP tetap untuk beacon & listen client */
#define LAN_BEACON      0x11  /* server -> broadcast: info + gameport */
#define LAN_BEACON_MS   3000  /* interval broadcast */
/* format: "ROMP" 0x11 gameport(u16) hostnamelen(u8) hostname
 *         players(u16) maxplayers(u16) has_password(u8) motdlen(u8) motd */

/* ---- Protokol master server (TCP, framing sama: u16 LE + type) ---- */
#define M_ANNOUNCE    0x50  /* server -> master: daftarkan server */
#define M_LIST_REQ    0x51  /* client -> master: minta daftar */
#define M_LIST        0x52  /* master -> client: daftar server */

#define CHAT_SERVER   0xFFFF
#define KIND_CHAT 0
#define KIND_INFO 1
#define KIND_ME   2
#define KIND_PM   3
#define KIND_SYS  4

#define LEAVE_QUIT 0
#define LEAVE_KICK 1
#define LEAVE_TIMEOUT 2

/* ---------------- Konfigurasi ---------------- */
typedef struct {
    int  port;
    char hostname[64];
    char motd[256];
    int  maxplayers;
    char password[64];
    char gamemode[256];   /* path file gamemode.lua */
    int  query;           /* 1 = jawab UDP query server browser (default 1) */
    int  announce;        /* 1 = announce periodik ke master server */
    char master_host[128];/* host master server (kosong = tidak announce) */
    int  master_port;     /* port master server (default 7778) */
    int  lan_discovery;   /* 1 = broadcast beacon LAN tiap 3 dtk (default 1) */
} Config;

extern Config g_cfg;

/* ---------------- Client ---------------- */
#define RECV_BUF 8192

typedef struct Client {
    SOCKET sock;
    int    id;                 /* -1 = belum handshake */
    char   name[25];
    char   ip[48];             /* IP remote (dari accept) */
    int    handshaked;
    uint8_t recv[RECV_BUF];
    size_t  recv_len;
    uint64_t last_activity;
    uint64_t last_pos_ms;
    uint64_t chat_times[3];    /* anti-spam: 3 pesan / 2 detik */
    int    chat_idx;
    int    dead;               /* ditandai untuk disconnect */
    int    kick_reason;
    struct Client *next;
} Client;

extern Client *g_clients;

/* ---------------- API core untuk dipakai scripting layer ---------------- */
void log_msg(const char *fmt, ...);
int client_count(void);
Client *client_by_id(int id);
Client *client_by_name(const char *name);
void chat_broadcast(uint16_t pid, uint8_t kind, const char *msg);
void chat_to(Client *c, uint16_t pid, uint8_t kind, const char *msg);
void server_say(const char *msg);
/* Kirim frame mentah ke client (dipakai scripting layer). 0 = sukses. */
int client_send_frame(Client *c, const uint8_t *payload, size_t n);
/* Kick pemain + umumkan. Dipakai console & Lua. */
void server_kick(int id, const char *reason);
uint64_t now_ms(void);
/* Daftar IP yang di-ban (in-memory). Dipakai Lua (banIP/unbanIP/isIPBanned). */
void ban_add(const char *ip);
void ban_remove(const char *ip);
void ban_clear(void);
int  ban_check(const char *ip);
/* Minta announce thread segera kirim announce ke master. Dipakai Lua. */
void trigger_announce(void);
