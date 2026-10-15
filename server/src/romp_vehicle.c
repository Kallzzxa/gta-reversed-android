/*
 * RO-MP vehicle sync ala SA-MP — implementasi registry kendaraan.
 * Lihat romp_vehicle.h untuk desain protokol & API.
 */
#include "romp_vehicle.h"
#include "romp_label.h"
#include "romp_lua.h"

static Vehicle *g_vehicles = NULL;

/* ---- Helper frame (duplikat kecil dari romp_server.c agar modul mandiri) ---- */
static void vput_u16(uint8_t *b, uint16_t v) { b[0] = v & 0xFF; b[1] = (v >> 8) & 0xFF; }
static void vput_f32(uint8_t *b, float v) { uint32_t u; memcpy(&u, &v, 4); vput_u16(b, (uint16_t)(u & 0xFFFF)); vput_u16(b + 2, (uint16_t)(u >> 16)); }
static uint16_t vget_u16(const uint8_t *b) { return (uint16_t)(b[0] | (b[1] << 8)); }
static float vget_f32(const uint8_t *b) {
    uint32_t u = (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
                 ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    float f; memcpy(&f, &u, 4); return f;
}

/* Tabel compid -> slot komponen ala SA-MP.
 * Data dari open.mp wiki via markski1/SAMP-vModData (lisensi MIT).
 * Slot 0..13 standar SA-MP, 14/15 bullbar (ekstensi vSync).
 */

/* compid 1000..1179 -> slot 0..15, -1 = invalid. */
/* Sumber data: open.mp wiki via markski1/SAMP-vModData (MIT). */
static const int8_t g_comp_slot[180] = {
     0, /* 1000 Pro spoiler                  */
     0, /* 1001 Win spoiler                  */
     0, /* 1002 Drag spoiler                 */
     0, /* 1003 Alpha spoiler                */
     1, /* 1004 Champhood                    */
     1, /* 1005 Fury hood                    */
     2, /* 1006 Scoop roof                   */
     3, /* 1007 Right Sideskirt              */
     5, /* 1008 5 times                      */
     5, /* 1009 2 times                      */
     5, /* 1010 10 times                     */
     1, /* 1011 Race hood                    */
     1, /* 1012 Worx hood                    */
     4, /* 1013 Round Fog                    */
     0, /* 1014 Champ spoiler                */
     0, /* 1015 Race spoiler                 */
     0, /* 1016 Worx spoiler                 */
     3, /* 1017 Left Sideskirt               */
     6, /* 1018 Upswept exhaust              */
     6, /* 1019 Twin exhaust                 */
     6, /* 1020 Large exhaust                */
     6, /* 1021 Medium exhaust               */
     6, /* 1022 Small exhaust                */
     0, /* 1023 Fury spoiler                 */
     4, /* 1024 Square Fog                   */
     7, /* 1025 Offroad                      */
     3, /* 1026 Right Alien Sideskirt        */
     3, /* 1027 Left Alien Sideskirt         */
     6, /* 1028 Alien exhaust                */
     6, /* 1029 X-Flow exhaust               */
     3, /* 1030 Left X-Flow Sideskirt        */
     3, /* 1031 Right X-Flow Sideskirt       */
     2, /* 1032 Alien roof                   */
     2, /* 1033 X-Flow roof                  */
     6, /* 1034 Alien exhaust                */
     2, /* 1035 X-Flow roof                  */
     3, /* 1036 Right Alien Sideskirt        */
     6, /* 1037 X-Flow exhaust               */
     2, /* 1038 Alien roof                   */
     3, /* 1039 Left X-Flow Sideskirt        */
     3, /* 1040 Left Alien Sideskirt         */
     3, /* 1041 Right X-Flow Sideskirt       */
     3, /* 1042 Right Chrome Sideskirt       */
     6, /* 1043 Slamin exhaust               */
     6, /* 1044 Chrome exhaust               */
     6, /* 1045 X-Flow exhaust               */
     6, /* 1046 Alien exhaust                */
     3, /* 1047 Right Alien Sideskirt        */
     3, /* 1048 Right X-Flow Sideskirt       */
     0, /* 1049 Alien spoiler                */
     0, /* 1050 X-Flow spoiler               */
     3, /* 1051 Left Alien Sideskirt         */
     3, /* 1052 Left X-Flow Sideskirt        */
     2, /* 1053 X-Flow roof                  */
     2, /* 1054 Alien roof                   */
     2, /* 1055 Alien roof                   */
     3, /* 1056 Right Alien Sideskirt        */
     3, /* 1057 Right X-Flow Sideskirt       */
     0, /* 1058 Alien spoiler                */
     6, /* 1059 X-Flow exhaust               */
     0, /* 1060 X-Flow spoiler               */
     2, /* 1061 X-Flow roof                  */
     3, /* 1062 Left Alien Sideskirt         */
     3, /* 1063 Left X-Flow Sideskirt        */
     6, /* 1064 Alien exhaust                */
     6, /* 1065 Alien exhaust                */
     6, /* 1066 X-Flow exhaust               */
     2, /* 1067 Alien roof                   */
     2, /* 1068 X-Flow roof                  */
     3, /* 1069 Right Alien Sideskirt        */
     3, /* 1070 Right X-Flow Sideskirt       */
     3, /* 1071 Left Alien Sideskirt         */
     3, /* 1072 Left X-Flow Sideskirt        */
     7, /* 1073 Shadow                       */
     7, /* 1074 Mega                         */
     7, /* 1075 Rimshine                     */
     7, /* 1076 Wires                        */
     7, /* 1077 Classic                      */
     7, /* 1078 Twist                        */
     7, /* 1079 Cutter                       */
     7, /* 1080 Switch                       */
     7, /* 1081 Grove                        */
     7, /* 1082 Import                       */
     7, /* 1083 Dollar                       */
     7, /* 1084 Trance                       */
     7, /* 1085 Atomic                       */
     8, /* 1086 Stereo                       */
     9, /* 1087 Hydraulics                   */
     2, /* 1088 Alien roof                   */
     6, /* 1089 X-Flow exhaust               */
     3, /* 1090 Right Alien Sideskirt        */
     2, /* 1091 X-Flow roof                  */
     6, /* 1092 Alien exhaust                */
     3, /* 1093 Right X-Flow Sideskirt       */
     3, /* 1094 Left Alien Sideskirt         */
     3, /* 1095 Right X-Flow Sideskirt       */
     7, /* 1096 Ahab                         */
     7, /* 1097 Virtual                      */
     7, /* 1098 Access                       */
     3, /* 1099 Left Chrome Sideskirt        */
    14, /* 1100 Chrome Grill                 */
     2, /* 1101 Covertible roof              */
     6, /* 1102 Chrome exhaust               */
     6, /* 1103 Slamin exhaust               */
    15, /* 1104 Chrome                       */
    15, /* 1105 Slamin                       */
    -1, /* 1106 Invalid                      */
    -1, /* 1107 Invalid                      */
     6, /* 1108 Chrome exhaust               */
     6, /* 1109 Slamin exhaust               */
    14, /* 1110 Chrome                       */
    14, /* 1111 Slamin                       */
    10, /* 1112 Chrome                       */
    14, /* 1113 Bullbar Chrome Bars          */
    14, /* 1114 Bullbar Chrome Lights        */
     6, /* 1115 Chrome exhaust               */
     6, /* 1116 Slamin exhaust               */
     2, /* 1117 Vinyl roof Hardtop           */
     6, /* 1118 Chrome exhaust               */
     2, /* 1119 Hardtop roof                 */
     2, /* 1120 Softtop roof                 */
     6, /* 1121 Slamin exhaust               */
     6, /* 1122 Slamin exhaust               */
     6, /* 1123 Chrome exhaust               */
     0, /* 1124 Alien spoiler                */
     0, /* 1125 X-Flow spoiler               */
    11, /* 1126 X-Flow rear bumper           */
    11, /* 1127 Alien rear bumper            */
    13, /* 1128 Left Oval Vents              */
    12, /* 1129 Right Oval Vents             */
    13, /* 1130 Left Square Vents            */
    12, /* 1131 Right Square Vents           */
     0, /* 1132 X-Flow spoiler               */
     0, /* 1133 Alien spoiler                */
    11, /* 1134 X-Flow rear bumper           */
    11, /* 1135 Alien rear bumper            */
    11, /* 1136 Alien rear bumper            */
    11, /* 1137 X-Flow rear bumper           */
    10, /* 1138 X-Flow front bumper          */
    10, /* 1139 Alien front bumper           */
    11, /* 1140 Alien rear bumper            */
    10, /* 1141 Alien front bumper           */
    11, /* 1142 X-Flow rear bumper           */
    10, /* 1143 X-Flow front bumper          */
     0, /* 1144 X-Flow spoiler               */
    11, /* 1145 Alien rear bumper            */
    10, /* 1146 Alien front bumper           */
    11, /* 1147 X-Flow rear bumper           */
     0, /* 1148 Alien spoiler                */
     0, /* 1149 X-Flow spoiler               */
     0, /* 1150 Alien spoiler                */
    10, /* 1151 X-Flow front bumper          */
    10, /* 1152 Alien front bumper           */
    11, /* 1153 X-Flow rear bumper           */
    11, /* 1154 Alien rear bumper            */
    10, /* 1155 Alien front bumper           */
    10, /* 1156 X-Flow front bumper          */
    10, /* 1157 Alien front bumper           */
    10, /* 1158 X-Flow front bumper          */
    10, /* 1159 X-Flow front bumper          */
    10, /* 1160 Chrome front bumper          */
    10, /* 1161 Slamin front bumper          */
    11, /* 1162 Chrome front bumper          */
    11, /* 1163 Slamin front bumper          */
    11, /* 1164 Slamin front bumper          */
    10, /* 1165 Chrome front bumper          */
    11, /* 1166 Chrome front bumper          */
    10, /* 1167 Slamin front bumper          */
    10, /* 1168 Chrome front bumper          */
    11, /* 1169 Slamin front bumper          */
    11, /* 1170 Chrome front bumper          */
    10, /* 1171 Slamin front bumper          */
    11, /* 1172 Slamin front bumper          */
    11, /* 1173 Chrome front bumper          */
    10, /* 1174 Slamin front bumper          */
    10, /* 1175 Chrome front bumper          */
    10, /* 1176 Slamin front bumper          */
    10, /* 1177 Chrome front bumper          */
    11, /* 1178 Chrome front bumper          */
    11, /* 1179 Slamin front bumper          */
};

/* Slot komponen untuk compid (0..15), -1 bila invalid/tak terdaftar. */
int vehicle_component_type(uint16_t compid) {
    if (compid < 1000 || compid > 1179) return -1;
    return (int)g_comp_slot[compid - 1000];
}

/* ---------------- Registry ---------------- */

Vehicle *vehicle_by_id(int id) {
    if (id <= 0) return NULL;
    for (Vehicle *v = g_vehicles; v; v = v->next)
        if (v->id == id) return v;
    return NULL;
}

int vehicle_count(void) {
    int n = 0;
    for (Vehicle *v = g_vehicles; v; v = v->next) n++;
    return n;
}

Vehicle *vehicle_first(void) { return g_vehicles; }
Vehicle *vehicle_next(Vehicle *v) { return v ? v->next : NULL; }

static int alloc_vid(void) {
    for (int id = 1; id <= VEH_MAX; id++)
        if (!vehicle_by_id(id)) return id;
    return -1;
}

/* ---------------- Broadcast paket kendaraan (vw-scoped ala SA-MP) ----------------
 * Kendaraan punya virtual world sendiri (default 0). Spawn/despawn/seat
 * hanya dikirim ke pemain yang vw-nya sama dengan kendaraan; relay posisi
 * (VEH_SYNC) hanya ke pemain yang terlihat oleh sopir (vw & interior sama).
 * Update driverless (teleport/respawn dari server) ke semua pemain di vw
 * kendaraan. */

/* 0x13 VEH_SYNC: u16 driverid | u16 vid | f32 x,y,z | f32 heading | u16 health.
 * driverid 0xFFFF = update server-side (teleport/respawn), bukan dari sopir. */
static void bcast_veh_sync(Vehicle *v, int driverid) {
    uint8_t b[24];
    b[0] = P_VEH_SYNC;
    vput_u16(b + 1, (uint16_t)(driverid < 0 ? 0xFFFF : driverid));
    vput_u16(b + 3, (uint16_t)v->id);
    vput_f32(b + 5, v->x);
    vput_f32(b + 9, v->y);
    vput_f32(b + 13, v->z);
    vput_f32(b + 17, v->heading);
    vput_u16(b + 21, v->health);
    if (driverid >= 0) {
        /* relay dari sopir: hanya yang terlihat oleh sopir */
        broadcast_frame_ref(b, 23, client_by_id(driverid), NULL);
    } else {
        broadcast_frame_vw(b, 23, NULL, v->vw);
    }
}

/* Isi frame 0x14 VEH_SPAWN untuk satu kendaraan. */
static size_t veh_spawn_frame(Vehicle *v, uint8_t *b) {
    b[0] = P_VEH_SPAWN;
    vput_u16(b + 1, (uint16_t)v->id);
    vput_u16(b + 3, v->model);
    vput_f32(b + 5, v->x);
    vput_f32(b + 9, v->y);
    vput_f32(b + 13, v->z);
    vput_f32(b + 17, v->sangle);
    b[21] = v->c1;
    b[22] = v->c2;
    vput_u16(b + 23, v->health);
    vput_u16(b + 25, (uint16_t)(v->driver < 0 ? 0xFFFF : v->driver));
    return 27;
}

/* 0x14 VEH_SPAWN: u16 vid | u16 model | f32 x,y,z | f32 angle |
 *                 u8 c1 | u8 c2 | u16 health | u16 driverid (0xFFFF = kosong) */
static void bcast_veh_spawn(Vehicle *v) {
    uint8_t b[27];
    broadcast_frame_vw(b, veh_spawn_frame(v, b), NULL, v->vw);
}

/* 0x15 VEH_DESPAWN: u16 vid */
static void bcast_veh_despawn(int vid, uint32_t vw) {
    uint8_t b[3];
    b[0] = P_VEH_DESPAWN;
    vput_u16(b + 1, (uint16_t)vid);
    broadcast_frame_vw(b, 3, NULL, vw);
}

/* 0x16 VEH_SEAT: u16 vid | u8 seat | u16 pid (0xFFFF = kosong) */
static void bcast_veh_seat(Vehicle *v, int seat, int pid) {
    uint8_t b[6];
    b[0] = P_VEH_SEAT;
    vput_u16(b + 1, (uint16_t)v->id);
    b[3] = (uint8_t)seat;
    vput_u16(b + 4, (uint16_t)(pid < 0 ? 0xFFFF : pid));
    broadcast_frame_vw(b, 6, NULL, v->vw);
}

/* 0x3A VEH_COMP: u16 vid | u16 compid | u8 slot */
static void bcast_veh_comp(Vehicle *v, uint16_t compid, int slot) {
    uint8_t b[6];
    b[0] = P_VEH_COMP;
    vput_u16(b + 1, (uint16_t)v->id);
    vput_u16(b + 3, compid);
    b[5] = (uint8_t)slot;
    broadcast_frame_vw(b, 6, NULL, v->vw);
}

/* 0x3B VEH_COMP_REMOVE: u16 vid | u16 compid */
static void bcast_veh_comp_remove(Vehicle *v, uint16_t compid) {
    uint8_t b[5];
    b[0] = P_VEH_COMP_REMOVE;
    vput_u16(b + 1, (uint16_t)v->id);
    vput_u16(b + 3, compid);
    broadcast_frame_vw(b, 5, NULL, v->vw);
}

/* 0x3C VEH_PAINTJOB: u16 vid | u8 paintjob */
static void bcast_veh_paintjob(Vehicle *v) {
    uint8_t b[4];
    b[0] = P_VEH_PAINTJOB;
    vput_u16(b + 1, (uint16_t)v->id);
    b[3] = v->paintjob;
    broadcast_frame_vw(b, 4, NULL, v->vw);
}

/* Kirim komponen + paintjob satu kendaraan ke satu client (resync join/pindah
 * vw). Dipanggil setelah 0x14 + 0x16 di send_veh_to. */
static void send_veh_mods_to(Client *c, Vehicle *v) {
    for (int s = 0; s < VEH_COMP_SLOTS; s++) {
        if (v->comp[s] == 0) continue;
        uint8_t b[6];
        b[0] = P_VEH_COMP;
        vput_u16(b + 1, (uint16_t)v->id);
        vput_u16(b + 3, v->comp[s]);
        b[5] = (uint8_t)s;
        client_send_frame(c, b, 6);
    }
    if (v->paintjob != VEH_PAINTJOB_NONE) {
        uint8_t b[4];
        b[0] = P_VEH_PAINTJOB;
        vput_u16(b + 1, (uint16_t)v->id);
        b[3] = v->paintjob;
        client_send_frame(c, b, 4);
    }
}

/* Kirim 0x14 (+ 0x16 untuk kursi terisi, + 0x3A/0x3C untuk modifikasi)
 * satu kendaraan ke satu client. */
static void send_veh_to(Client *c, Vehicle *v) {
    uint8_t b[27];
    client_send_frame(c, b, veh_spawn_frame(v, b));
    for (int s = 0; s < VEH_SEATS; s++) {
        int occ = vehicle_occupant(v->id, s);
        if (occ < 0) continue;
        uint8_t sb[6];
        sb[0] = P_VEH_SEAT;
        vput_u16(sb + 1, (uint16_t)v->id);
        sb[3] = (uint8_t)s;
        vput_u16(sb + 4, (uint16_t)occ);
        client_send_frame(c, sb, 6);
    }
    send_veh_mods_to(c, v);
}

/* ---------------- API inti ---------------- */

int vehicle_create(uint16_t model, float x, float y, float z, float angle,
                   uint8_t c1, uint8_t c2, uint32_t respawn_ms) {
    /* id model kendaraan SA-MP: 400..611 */
    if (model < 400 || model > 611) return -1;
    int id = alloc_vid();
    if (id < 0) return -1;
    Vehicle *v = (Vehicle *)calloc(1, sizeof(Vehicle));
    if (!v) return -1;
    v->id = id;
    v->model = model;
    v->sx = v->x = x; v->sy = v->y = y; v->sz = v->z = z;
    v->sangle = v->heading = angle;
    v->c1 = c1; v->c2 = c2;
    v->health = VEH_HEALTH_MAX;
    v->driver = -1;
    v->pass[0] = v->pass[1] = v->pass[2] = -1;
    v->paintjob = VEH_PAINTJOB_NONE; /* comp[] = 0 via calloc */
    v->respawn_ms = respawn_ms;
    v->next = g_vehicles;
    g_vehicles = v;
    bcast_veh_spawn(v);
    log_msg("[veh] dibuat id %d (model %u) di %.1f, %.1f, %.1f", id, model, x, y, z);
    return id;
}

/* Keluarkan semua penumpang; fire_exit=1 -> event Lua, 0 -> diam. */
static void eject_all(Vehicle *v, int fire_exit) {
    int pids[VEH_SEATS];
    pids[0] = v->driver;
    pids[1] = v->pass[0]; pids[2] = v->pass[1]; pids[3] = v->pass[2];
    v->driver = -1;
    v->pass[0] = v->pass[1] = v->pass[2] = -1;
    for (int s = 0; s < VEH_SEATS; s++) {
        if (pids[s] < 0) continue;
        Client *c = client_by_id(pids[s]);
        if (c) { c->vehicle_id = -1; c->vehicle_seat = -1; }
        bcast_veh_seat(v, s, -1);
        if (fire_exit) lua_on_player_exit_vehicle(pids[s], v->id);
    }
}

int vehicle_destroy(int id) {
    Vehicle **pp = &g_vehicles;
    while (*pp) {
        if ((*pp)->id == id) {
            Vehicle *v = *pp;
            eject_all(v, 1);
            /* 3D text label yang di-attach ikut terhapus (ala SA-MP) */
            label_on_vehicle_destroy(id);
            bcast_veh_despawn(id, v->vw);
            *pp = v->next;
            free(v);
            log_msg("[veh] dihancurkan id %d", id);
            return 1;
        }
        pp = &(*pp)->next;
    }
    return 0;
}

/* Urutan mati: tandai destroyed, keluarkan penumpang, broadcast, event. */
static void vehicle_kill(Vehicle *v, int killerid) {
    if (v->destroyed) return;
    v->destroyed = 1;
    v->dead_at = now_ms();
    v->health = 0;
    eject_all(v, 1);
    bcast_veh_sync(v, -1);
    log_msg("[veh] id %d hancur", v->id);
    lua_on_vehicle_death(v->id, killerid);
}

int vehicle_set_pos(int id, float x, float y, float z, float angle) {
    Vehicle *v = vehicle_by_id(id);
    if (!v || v->destroyed) return 0;
    v->x = x; v->y = y; v->z = z; v->heading = angle;
    bcast_veh_sync(v, -1); /* driverless = otoritatif dari server */
    return 1;
}

int vehicle_set_health(int id, uint16_t health, int killerid) {
    Vehicle *v = vehicle_by_id(id);
    if (!v) return 0;
    if (health > VEH_HEALTH_MAX) health = VEH_HEALTH_MAX;
    if (v->destroyed) {
        /* Perbaiki kendaraan yang hancur = hidup lagi di tempat. */
        if (health <= VEH_HEALTH_WRECK) return 1;
        v->destroyed = 0;
        v->health = health;
        bcast_veh_sync(v, -1);
        lua_on_vehicle_spawn(v->id);
        return 1;
    }
    if (health == v->health) return 1;
    if (health < v->health)
        lua_on_vehicle_damage(v->id, killerid, health);
    v->health = health;
    if (health <= VEH_HEALTH_WRECK) {
        vehicle_kill(v, killerid);
        return 1;
    }
    bcast_veh_sync(v, v->driver);
    return 1;
}

int vehicle_damage(int id, uint16_t amount, int killerid) {
    Vehicle *v = vehicle_by_id(id);
    if (!v || v->destroyed) return -1;
    uint16_t nh = v->health > amount ? (uint16_t)(v->health - amount) : 0;
    vehicle_set_health(id, nh, killerid);
    return (int)v->health;
}

int vehicle_respawn(int id) {
    Vehicle *v = vehicle_by_id(id);
    if (!v) return 0;
    v->destroyed = 0;
    v->health = VEH_HEALTH_MAX;
    v->x = v->sx; v->y = v->sy; v->z = v->sz; v->heading = v->sangle;
    bcast_veh_sync(v, -1);
    lua_on_vehicle_spawn(v->id);
    log_msg("[veh] id %d di-respawn", id);
    return 1;
}

int vehicle_set_colors(int id, uint8_t c1, uint8_t c2) {
    Vehicle *v = vehicle_by_id(id);
    if (!v) return 0;
    v->c1 = c1; v->c2 = c2;
    bcast_veh_spawn(v); /* spawn ulang di client = warna baru */
    return 1;
}

/* ---------------- Komponen kendaraan ala SA-MP (tuning) ---------------- */

int vehicle_add_component(int vid, uint16_t compid) {
    Vehicle *v = vehicle_by_id(vid);
    if (!v || v->destroyed) return 0;
    int slot = vehicle_component_type(compid);
    if (slot < 0 || slot >= VEH_COMP_SLOTS) return 0;
    v->comp[slot] = compid; /* menimpa yang lama di slot yang sama (ala SA-MP) */
    bcast_veh_comp(v, compid, slot);
    log_msg("[veh] komponen %u (slot %d) dipasang di kendaraan %d",
            compid, slot, vid);
    return 1;
}

int vehicle_remove_component(int vid, uint16_t compid) {
    Vehicle *v = vehicle_by_id(vid);
    if (!v || v->destroyed) return 0;
    int slot = vehicle_component_type(compid);
    if (slot < 0 || slot >= VEH_COMP_SLOTS) return 0;
    if (v->comp[slot] != compid) return 0; /* tidak terpasang */
    v->comp[slot] = 0;
    bcast_veh_comp_remove(v, compid);
    log_msg("[veh] komponen %u dilepas dari kendaraan %d", compid, vid);
    return 1;
}

int vehicle_get_component_in_slot(int vid, int slot) {
    Vehicle *v = vehicle_by_id(vid);
    if (!v || slot < 0 || slot >= VEH_COMP_SLOTS) return -1;
    if (v->comp[slot] == 0) return -1;
    return (int)v->comp[slot];
}

int vehicle_set_paintjob(int vid, uint8_t paintjob) {
    Vehicle *v = vehicle_by_id(vid);
    if (!v || v->destroyed) return 0;
    if (paintjob > 2 && paintjob != VEH_PAINTJOB_NONE) return 0;
    v->paintjob = paintjob;
    bcast_veh_paintjob(v);
    return 1;
}

int vehicle_get_paintjob(int vid) {
    Vehicle *v = vehicle_by_id(vid);
    if (!v) return -1;
    return (int)v->paintjob;
}

/* ---------------- Penumpang ---------------- */

int vehicle_put_player(int pid, int vid, int seat) {
    Client *c = client_by_id(pid);
    Vehicle *v = vehicle_by_id(vid);
    if (!c || !v || v->destroyed) return 0;
    if (c->is_npc) return 0; /* NPC belum bisa naik kendaraan (v1) */
    if (seat < 0 || seat >= VEH_SEATS) return 0;
    if (c->vehicle_id == vid && c->vehicle_seat == seat) return 1;
    int taken = (seat == 0) ? v->driver : v->pass[seat - 1];
    if (taken >= 0) return 0; /* kursi sudah ada yang duduki */
    if (c->vehicle_id >= 0)
        vehicle_remove_player(pid); /* pindah kendaraan: keluar dulu */
    if (seat == 0) v->driver = pid;
    else v->pass[seat - 1] = pid;
    c->vehicle_id = vid;
    c->vehicle_seat = seat;
    bcast_veh_seat(v, seat, pid);
    lua_on_player_enter_vehicle(pid, vid, seat);
    return 1;
}

static int remove_player_impl(int pid, int fire_exit) {
    Client *c = client_by_id(pid);
    if (!c || c->vehicle_id < 0) return 0;
    Vehicle *v = vehicle_by_id(c->vehicle_id);
    int vid = c->vehicle_id;
    int seat = c->vehicle_seat;
    c->vehicle_id = -1;
    c->vehicle_seat = -1;
    if (v) {
        if (seat == 0 && v->driver == pid) v->driver = -1;
        else if (seat >= 1 && seat < VEH_SEATS && v->pass[seat - 1] == pid)
            v->pass[seat - 1] = -1;
        bcast_veh_seat(v, seat, -1);
    }
    if (fire_exit) lua_on_player_exit_vehicle(pid, vid);
    return 1;
}

int vehicle_remove_player(int pid) {
    return remove_player_impl(pid, 1);
}

void vehicle_remove_player_silent(int pid) {
    remove_player_impl(pid, 0);
}

int vehicle_of_player(int pid) {
    Client *c = client_by_id(pid);
    if (!c) return -1;
    return c->vehicle_id;
}

int vehicle_seat_of_player(int pid) {
    Client *c = client_by_id(pid);
    if (!c || c->vehicle_id < 0) return -1;
    return c->vehicle_seat;
}

int vehicle_occupant(int vid, int seat) {
    Vehicle *v = vehicle_by_id(vid);
    if (!v || seat < 0 || seat >= VEH_SEATS) return -1;
    return (seat == 0) ? v->driver : v->pass[seat - 1];
}

/* ---------------- Sync jaringan ---------------- */

/* Paket 0x12 VEH dari client: u16 vid | f32 x,y,z | f32 heading | u16 health.
 * Hanya sopir yang boleh sync; selain itu diabaikan (anti-cheat dasar). */
void vehicle_on_packet(Client *c, const uint8_t *p, size_t n) {
    if (!c->handshaked || n < 20) return;
    int vid = (int)vget_u16(p);
    Vehicle *v = vehicle_by_id(vid);
    if (!v || v->destroyed) return;
    if (v->driver != c->id || c->vehicle_id != vid || c->vehicle_seat != 0)
        return; /* bukan sopir: abaikan */
    uint64_t t = now_ms();
    if (t - v->last_sync_ms < 30) return; /* rate limit ~33 Hz */
    v->last_sync_ms = t;
    v->x = vget_f32(p + 2);
    v->y = vget_f32(p + 6);
    v->z = vget_f32(p + 10);
    v->heading = vget_f32(p + 14);
    uint16_t health = vget_u16(p + 18);
    if (health > VEH_HEALTH_MAX) health = VEH_HEALTH_MAX;
    if (health != v->health) {
        /* Damage/heal yang dilaporkan sopir lewat sync. */
        vehicle_set_health(vid, health, -1);
        return; /* vehicle_set_health sudah broadcast */
    }
    bcast_veh_sync(v, c->id);
}

/* Stream kendaraan ke pemain yang baru join / pindah vw:
 * hanya kendaraan yang vw-nya sama dengan pemain. */
void vehicle_sync_join(Client *c) {
    for (Vehicle *v = g_vehicles; v; v = v->next) {
        if (v->destroyed) continue;
        if (v->vw != c->vw) continue;
        send_veh_to(c, v);
    }
}

/* Resync kendaraan saat pemain pindah vw: despawn yang vw lama,
 * spawn yang vw baru. */
void vehicle_sync_world(Client *c, uint32_t old_vw) {
    uint8_t d[3];
    d[0] = P_VEH_DESPAWN;
    for (Vehicle *v = g_vehicles; v; v = v->next) {
        if (v->destroyed) continue;
        if (v->vw == old_vw && old_vw != c->vw) {
            vput_u16(d + 1, (uint16_t)v->id);
            client_send_frame(c, d, 3);
        }
    }
    vehicle_sync_join(c);
}

/* Pindahkan kendaraan ke virtual world lain ala SA-MP
 * (SetVehicleVirtualWorld). Broadcast despawn ke viewer vw lama,
 * spawn ke viewer vw baru. 1 = sukses. */
int vehicle_set_vw(int id, uint32_t vw) {
    Vehicle *v = vehicle_by_id(id);
    if (!v) return 0;
    if (v->vw == vw) return 1;
    uint32_t old = v->vw;
    bcast_veh_despawn(v->id, old);
    v->vw = vw;
    bcast_veh_spawn(v);
    /* 3D text label yang di-attach ikut pindah vw */
    label_sync_vehicle_world(v->id, old);
    for (int s = 0; s < VEH_SEATS; s++) {
        int occ = vehicle_occupant(v->id, s);
        if (occ >= 0) bcast_veh_seat(v, s, occ);
    }
    log_msg("[veh] id %d pindah vw %u -> %u", id, old, vw);
    return 1;
}

void vehicle_tick(uint64_t now) {
    for (Vehicle *v = g_vehicles; v; v = v->next) {
        if (v->destroyed && v->respawn_ms > 0 &&
            now - v->dead_at >= v->respawn_ms) {
            v->destroyed = 0;
            v->health = VEH_HEALTH_MAX;
            v->x = v->sx; v->y = v->sy; v->z = v->sz; v->heading = v->sangle;
            bcast_veh_sync(v, -1);
            log_msg("[veh] id %d respawn otomatis", v->id);
            lua_on_vehicle_spawn(v->id);
        }
    }
}
