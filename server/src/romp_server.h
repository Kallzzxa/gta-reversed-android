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
#define P_INT_VW      0x25  /* server -> client: interior(u8) + vw(u32 LE) pemain */
#define P_STAT        0x26  /* client -> server: laporan health/armour client
                           * (u16 health, u16 armour) untuk anti-cheat */
#define P_POS_SET     0x27  /* server -> client: teleport pemain lokal ke
                           * float x,y,z (lihat player_set_pos) */
#define P_PING        0x30
#define P_PONG        0x31
#define P_NUI_MSG     0x40
#define P_NUI_CB      0x41
#define P_NUI_FOCUS   0x42

/* ---- Sinkronisasi kendaraan ala SA-MP (lihat PROTOCOL.md) ---- */
#define P_VEH         0x12  /* client -> server: sync posisi dari sopir */
#define P_VEH_SYNC    0x13  /* server -> client: relay posisi kendaraan */
#define P_VEH_SPAWN   0x14  /* server -> client: kendaraan muncul */
#define P_VEH_DESPAWN 0x15  /* server -> client: kendaraan hilang */
#define P_VEH_SEAT    0x16  /* server -> client: perubahan penumpang */
#define P_VEH_COMP    0x3A  /* server -> client: komponen dipasang */
#define P_VEH_COMP_REMOVE 0x3B /* server -> client: komponen dilepas */
#define P_VEH_PAINTJOB 0x3C /* server -> client: paintjob berubah */
#define P_VEH_PARAMS  0x3D  /* server -> client: params berubah (SetVehicleParamsEx) */
#define P_VEH_PARAMS_P 0x3E /* server -> client: params per-pemain
                               (SetVehicleParamsForPlayer), hanya ke 1 pemain */

/* ---- Attached object ala SA-MP (lihat romp_attach.h & PROTOCOL.md) ---- */
#define P_ATTACH_OBJ  0x3F  /* server -> client: set/remove attached object */

/* ---- Weapon skill ala SA-MP (lihat romp_skill.h & PROTOCOL.md) ---- */
#define P_SKILL_SET   0x43  /* server -> client: level skill pemain berubah */

/* ---- Player marker ala SA-MP (lihat romp_marker.h & PROTOCOL.md) ---- */
#define P_MARKER_SET  0x44  /* server -> client: warna marker pemain (pid+u32) */
#define P_MARKER_MODE 0x45  /* server -> client: mode marker global (u8) */

/* ---- Kill feed ala SA-MP (lihat romp_death.h & PROTOCOL.md) ---- */
#define P_DEATH_REPORT 0x46 /* client -> server: laporan kematian (killerid+reason) */
#define P_DEATH_MSG    0x47 /* server -> semua client: kill feed (killer+victim+reason) */

/* ---- Wanted level ala SA-MP (lihat romp_wanted.h & PROTOCOL.md) ---- */
#define P_WANTED_SET  0x48  /* server -> semua client: wanted pemain berubah
                               (pid+level, global) */

/* ---- Sinkronisasi senjata & tembakan ala SA-MP (lihat PROTOCOL.md) ---- */
#define P_WPN         0x17  /* client -> server: event tembakan */
#define P_WPN_SYNC    0x18  /* server -> client: broadcast tembakan */
#define P_WPN_SET     0x19  /* server -> client: set slot senjata pemain */

/* ---- 3D text label ala SA-MP (lihat romp_label.h & PROTOCOL.md) ---- */
#define P_LABEL_SPAWN   0x28  /* server -> client: label muncul */
#define P_LABEL_DESPAWN 0x29  /* server -> client: label hilang */
#define P_LABEL_TEXT    0x2A  /* server -> client: ubah teks & warna label */
#define P_LABEL_POS     0x2B  /* server -> client: pindahkan label statis */

/* ---- Objek ala SA-MP (lihat romp_object.h & PROTOCOL.md) ---- */
#define P_OBJECT_SPAWN   0x2C  /* server -> client: objek muncul */
#define P_OBJECT_DESPAWN 0x2D  /* server -> client: objek hilang */
#define P_OBJECT_MOVE    0x2E  /* server -> client: objek bergerak (MoveObject) */
#define P_OBJECT_UPDATE  0x2F  /* server -> client: set posisi/rotasi instan */

/* ---- Actor statis ala SA-MP (lihat romp_actor.h & PROTOCOL.md) ---- */
#define P_ACTOR_SPAWN   0x32  /* server -> client: actor muncul */
#define P_ACTOR_DESPAWN 0x33  /* server -> client: actor hilang */
#define P_ACTOR_POS     0x34  /* server -> client: pindah actor (set pos+angle) */
#define P_ACTOR_ANIM    0x35  /* server -> client: terapkan animasi actor */
#define P_ACTOR_HEALTH  0x36  /* server -> client: health actor berubah */

/* ---- Audio stream ala SA-MP (lihat romp_audio.h & PROTOCOL.md) ---- */
#define P_AUDIO_PLAY    0x37  /* server -> client: putar URL audio */
#define P_AUDIO_STOP    0x38  /* server -> client: hentikan audio */

/* ---- World settings ala SA-MP (lihat romp_world.h & PROTOCOL.md) ---- */
#define P_WORLD_SET     0x39  /* server -> client: gravity/weather/time dunia */

/* ---- Protokol UDP query (server browser), port sama dengan TCP ---- */
#define QUERY_MAGIC   "ROMP"
#define QUERY_MAGIC_LEN 4
#define Q_INFO          0x01  /* client -> server: minta info */
#define Q_INFO_REPLY    0x02  /* server -> client: info server */
#define Q_PLAYERS       0x03  /* client -> server: minta daftar pemain */
#define Q_PLAYERS_REPLY 0x04  /* server -> client: daftar pemain */

/* ---- RCON (remote console ala SA-MP), UDP di port game ----
 * 0x05 RCON (client -> server):
 *   "ROMP" 0x05 pwlen(u8) pw cmdlen(u16 LE) cmd
 * 0x06 RCON_REPLY (server -> client):
 *   "ROMP" 0x06 status(u8: 0=OK, 1=password salah/RCON mati)
 *   outlen(u16 LE) out (teks output, baris dipisah '\n') */
#define R_RCON        0x05
#define R_RCON_REPLY  0x06

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
    char filterscripts[512]; /* daftar nama filterscript ala SA-MP, dipisah spasi */
    int  query;           /* 1 = jawab UDP query server browser (default 1) */
    int  announce;        /* 1 = announce periodik ke master server */
    char master_host[128];/* host master server (kosong = tidak announce) */
    int  master_port;     /* port master server (default 7778) */
    int  lan_discovery;   /* 1 = broadcast beacon LAN tiap 3 dtk (default 1) */
    int  rcon;           /* 1 = RCON via UDP aktif (default 1) */
    char rcon_password[64]; /* password RCON; kosong = RCON mati (ala SA-MP) */
    int  anticheat;      /* 1 = anti-cheat dasar aktif (default 1) */
    int  ac_maxwarnings;  /* peringatan sebelum auto-kick, 0 = tidak pernah kick */
    char database[256];   /* path file SQLite gamemode; kosong = fitur DB mati */
    float gravity;        /* world setting awal (ala SA-MP SetGravity; <=0 = default) */
    int   weather;        /* world setting awal (ala SA-MP SetWeather; <0 = default) */
    int   world_hour;     /* jam awal dunia (ala SA-MP SetWorldTime; <0 = default) */
    int   world_minute;   /* menit awal dunia (<0 = default) */
} Config;

extern Config g_cfg;

/* ---------------- Attached object ala SA-MP ----------------
 * Satu slot = satu objek yang menempel di bone pemain
 * (lihat romp_attach.h). Maks 10 slot per pemain, index 0..9
 * (ala SA-MP MAX_PLAYER_ATTACHED_OBJECTS). Bone 1..18 ala SA-MP:
 * 1=spine, 2=head, 3=l upper arm, 4=r upper arm, 5=l hand, 6=r hand,
 * 7=l thigh, 8=r thigh, 9=l foot, 10=r foot, 11=r calf, 12=l calf,
 * 13=l forearm, 14=r forearm, 15=l clavicle, 16=r clavicle, 17=neck,
 * 18=jaw. */
#define ATTACH_MAX_SLOTS 10

/* Jumlah skill senjata ala SA-MP (lihat romp_skill.h): id 0..10. */
#define SKILL_COUNT 11

/* Override warna marker per-viewer ala SA-MP SetPlayerMarkerForPlayer
 * (lihat romp_marker.h): viewer melihat pemain `pid` dengan `color`
 * (0xRRGGBBAA), menimpa warna milik target. */
typedef struct {
    int      pid;
    uint32_t color;
} MarkerOverride;
typedef struct {
    int      used;      /* 1 = slot terisi */
    uint32_t modelid;   /* model objek SA-MP, != 0 */
    uint8_t  bone;      /* 1..18 */
    float    off[3];    /* offset posisi dari bone (x,y,z) */
    float    rot[3];    /* rotasi euler derajat (x,y,z) */
    float    scale[3];  /* skala per sumbu, > 0 */
} AttachSlot;

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
    /* Posisi dunia terakhir dari paket 0x10 (dipakai checkpoint, dsb). */
    int    pos_known;          /* 1 = pernah terima paket POS */
    float  pos_x, pos_y, pos_z;
    /* Grace anti-cheat setelah server memindahkan pemain (setPlayerPos):
     * paket 0x10 yang masih dalam perjalanan dari posisi lama tidak boleh
     * memicu deteksi teleport. 0 = tak ada grace aktif. */
    uint64_t pos_grace_ms;
    uint64_t chat_times[3];    /* anti-spam: 3 pesan / 2 detik */
    int    chat_idx;
    int    dead;               /* ditandai untuk disconnect */
    int    kick_reason;
    /* NPC/bot ala SA-MP (ConnectNPC): koneksi virtual tanpa socket.
     * 1 = NPC: tak pernah di-pump, tak kena timeout, tak menerima paket
     * (send_frame jadi no-op), tapi muncul di daftar pemain, chat, dan
     * relay posisi seperti pemain biasa. */
    int    is_npc;
    /* Interior & virtual world ala SA-MP (default 0/0). */
    uint8_t  interior;
    uint32_t vw;
    /* Kendaraan yang ditumpangi (-1 = tidak di kendaraan; seat 0 = sopir). */
    int    vehicle_id;
    int    vehicle_seat;
    /* Inventory senjata ala SA-MP (otoritatif server; lihat romp_weapon.h). */
    uint16_t wpn[13];      /* weaponid per slot, 0 = kosong */
    uint16_t wpn_ammo[13]; /* ammo per slot */
    int    wpn_slot;       /* slot senjata aktif terakhir */
    uint64_t last_shot_ms; /* anti-spam tembakan */
    /* Ping ala SA-MP (scoreboard TAB): diukur server via probe P_PING.
     * -1 = belum terukur (mis. NPC / client tak menjawab probe). */
    int      ping_ms;          /* hasil RTT terakhir, milidetik */
    uint64_t ping_probe_ms;    /* 0 = tidak ada probe aktif */
    uint32_t ping_probe_tick;  /* u32 tick yang dikirim di probe terakhir */
    /* Anti-cheat dasar ala SA-MP (lihat romp_anticheat.h): health/armour
     * otoritatif server + jumlah peringatan cheat pemain. */
    float    ac_health;      /* default 100 */
    float    ac_armour;      /* default 0 */
    uint64_t ac_set_ms;      /* timestamp terakhir server set health/armour */
    int      ac_warnings;    /* jumlah peringatan cheat */
    int      ac_stat_known;  /* 1 = client pernah kirim P_STAT */
    /* Audio stream ala SA-MP (lihat romp_audio.h): satu stream per pemain,
     * otoritatif server. Client memutar URL (0x37) & berhenti via 0x38. */
    int      audio_playing;  /* 1 = ada stream aktif */
    char     audio_url[256]; /* URL yang sedang diputar (maks 255 byte) */
    int      audio_usepos;   /* 1 = posisional (3D), 0 = global */
    float    audio_x, audio_y, audio_z; /* posisi sumber bila posisional */
    float    audio_dist;     /* jarak dengar maksimal bila posisional */
    int      audio_loop;     /* 1 = putar ulang terus */
    /* Attached object ala SA-MP (lihat romp_attach.h): 10 slot per pemain,
     * otoritatif server. Di-broadcast vw-scoped (0x3F) ke semua pemain di
     * vw pemilik, termasuk pemiliknya sendiri. */
    AttachSlot attach[ATTACH_MAX_SLOTS];
    /* Weapon skill ala SA-MP (lihat romp_skill.h): level 0..1000 per
     * skill, otoritatif server, default SKILL_DEFAULT_LEVEL. Per-pemain:
     * 0x43 SKILL_SET hanya dikirim ke pemiliknya sendiri. */
    int      skill[SKILL_COUNT];
    /* Player marker ala SA-MP (lihat romp_marker.h): warna marker radar
     * milik pemain ini (0xRRGGBBAA, default dari palet otomatis) +
     * override warna per-viewer ala SetPlayerMarkerForPlayer
     * (array dinamis kecil — override jarang dipakai). */
    uint32_t marker_color;
    MarkerOverride *marker_ovs;
    int      marker_novs;
    int      marker_capovs;
    /* Wanted level ala SA-MP (lihat romp_wanted.h): 0 = bersih .. 6 =
     * paling dicari. Otoritatif server, default 0, di-broadcast global
     * (0x48 WANTED_SET) ke semua pemain. */
    uint8_t  wanted_level;
    struct Client *next;
} Client;

extern Client *g_clients;

/* ---------------- API core untuk dipakai scripting layer ---------------- */
void log_msg(const char *fmt, ...);
int client_count(void);
Client *client_by_id(int id);
Client *client_by_name(const char *name);
void chat_broadcast(uint16_t pid, uint8_t kind, const char *msg);
void chat_broadcast_scoped(Client *ref, uint16_t pid, uint8_t kind, const char *msg);
void chat_to(Client *c, uint16_t pid, uint8_t kind, const char *msg);
void server_say(const char *msg);
/* Kirim frame mentah ke client (dipakai scripting layer). 0 = sukses. */
int client_send_frame(Client *c, const uint8_t *payload, size_t n);
/* Broadcast frame ke semua client yang handshake (kecuali `except`). */
void broadcast_frame(const uint8_t *payload, size_t n, Client *except);
/* Broadcast frame hanya ke client yang "melihat" `ref` (vw & interior sama).
 * `except` dikecualikan. Dipakai relay posisi/chat/tembakan ala SA-MP. */
void broadcast_frame_ref(const uint8_t *payload, size_t n, Client *ref, Client *except);
/* Broadcast frame hanya ke client dengan virtual world tertentu
 * (interior diabaikan). Dipakai paket kendaraan ala SA-MP. */
void broadcast_frame_vw(const uint8_t *payload, size_t n, Client *except, uint32_t vw);
/* 1 = a dan b saling terlihat (vw & interior sama), 0 = tidak. */
int client_visible(const Client *a, const Client *b);
/* Kirim paket 0x25 (interior+vw) ke pemain. */
void send_int_vw(Client *c);
/* Ganti interior & vw pemain; lakukan resync streaming (P_JOIN/P_LEAVE/
 * kendaraan) terhadap pemain lain. Dipakai Lua & test. */
void player_set_world(Client *c, int interior, uint32_t vw);
/* Kick pemain + umumkan. Dipakai console & Lua. */
void server_kick(int id, const char *reason);
uint64_t now_ms(void);
/* ---------------- NPC/bot ala SA-MP (ConnectNPC) ----------------
 * Koneksi virtual untuk testing & populasi server. NPC menempati slot
 * pemain (client_count), muncul di P_LIST/daftar pemain/query, bisa chat
 * dan gerak via Lua, tapi tidak punya socket: tak menerima paket apa pun
 * dan tak pernah timeout. Event Lua onPlayerJoin/Leave/Chat ikut menyala
 * untuk NPC; gamemode membedakan via romp.isNPC(id). */
int  npc_connect(const char *name);   /* return id, -1 = gagal */
void npc_disconnect(int id, int reason);
int  npc_set_pos(int id, float x, float y, float z); /* broadcast 0x11 */
int  npc_send_chat(int id, const char *text);
int  npc_count(void);
/* Teleport pemain ala SA-MP SetPlayerPos. Update posisi otoritatif server,
 * kirim 0x27 P_POS_SET ke client target (NPC di-routing ke npc_set_pos),
 * dan broadcast 0x11 P_PPOS ke pemain yang melihatnya. Paket 0x10 yang
 * masih dalam perjalanan dari posisi lama diabaikan anti-cheat selama
 * AC_POS_GRACE_MS. 1 = sukses, 0 = id tidak valid. */
int  player_set_pos(int id, float x, float y, float z);
/* Daftar IP yang di-ban (in-memory). Dipakai Lua (banIP/unbanIP/isIPBanned). */
void ban_add(const char *ip);
void ban_remove(const char *ip);
void ban_clear(void);
int  ban_check(const char *ip);
/* Ambil IP ban ke-idx (0-based). 1 = ada, 0 = habis. Dipakai `bans`. */
int  ban_get(int idx, char *ip_out, size_t n);
/* Minta announce thread segera kirim announce ke master. Dipakai Lua. */
void trigger_announce(void);
