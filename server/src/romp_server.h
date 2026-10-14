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
} Config;

extern Config g_cfg;

/* ---------------- Client ---------------- */
#define RECV_BUF 8192

typedef struct Client {
    SOCKET sock;
    int    id;                 /* -1 = belum handshake */
    char   name[25];
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
/* Kick pemain + umumkan. Dipakai console & Lua. */
void server_kick(int id, const char *reason);
uint64_t now_ms(void);
