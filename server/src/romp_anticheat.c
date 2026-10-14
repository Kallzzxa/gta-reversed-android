/*
 * RO-MP anti-cheat dasar ala SA-MP.
 * (deskripsi lengkap di romp_anticheat.h)
 */
#include "romp_server.h"
#include "romp_lua.h"
#include "romp_anticheat.h"

#include <math.h>

/* Laporkan temuan cheat untuk pemain c. */
void ac_report(struct Client *c, const char *type, double value) {
    if (!c || !c->handshaked) return;
    c->ac_warnings++;
    log_msg("[anticheat] %s (id %d, %s): %s = %.1f (peringatan %d)",
            c->name, c->id, c->ip, type, value, c->ac_warnings);
    /* Event ke gamemode & filterscript ala SA-MP:
     * onPlayerCheat(playerid, type, value). Gamemode bebas mengambil
     * tindakan sendiri; server tetap menghitung peringatan otomatis. */
    lua_on_player_cheat(c->id, type, value);
    char msg[192];
    if (g_cfg.ac_maxwarnings > 0)
        snprintf(msg, sizeof(msg),
                 "*** Anti-cheat: %s terdeteksi (%s). Peringatan %d/%d.",
                 c->name, type, c->ac_warnings, g_cfg.ac_maxwarnings);
    else
        snprintf(msg, sizeof(msg),
                 "*** Anti-cheat: %s terdeteksi (%s). Peringatan %d.",
                 c->name, type, c->ac_warnings);
    chat_broadcast(CHAT_SERVER, KIND_SYS, msg);
    if (g_cfg.ac_maxwarnings > 0 && c->ac_warnings >= g_cfg.ac_maxwarnings) {
        log_msg("[anticheat] %s (id %d) di-kick otomatis: %d peringatan.",
                c->name, c->id, c->ac_warnings);
        server_kick(c->id, "Anti-cheat");
    }
}

int ac_check_movement(struct Client *c, float nx, float ny, float nz,
                      uint64_t now) {
    if (!g_cfg.anticheat || !c || c->is_npc) return 0;
    if (!c->pos_known) return 0;  /* posisi pertama: belum ada pembanding */
    uint64_t dt = now - c->last_pos_ms;
    if (dt == 0) return 0;
    float dx = nx - c->pos_x, dy = ny - c->pos_y, dz = nz - c->pos_z;
    float dist = sqrtf(dx * dx + dy * dy + dz * dz);
    if (dist >= AC_TELEPORT_DIST) {
        ac_report(c, "teleport", (double)dist);
        return 1; /* buang paket: posisi curang tidak disimpan */
    }
    /* Batas kecepatan: sopir pakai batas kendaraan, yang lain batas kaki. */
    float lim = (c->vehicle_id >= 0 && c->vehicle_seat == 0)
                ? AC_SPEED_VEH : AC_SPEED_FOOT;
    if (dt >= 80) {
        float speed = dist / ((float)dt / 1000.0f);
        if (speed > lim)
            ac_report(c, "speed", (double)speed);
    }
    return 0;
}

void ac_check_stat(struct Client *c, uint16_t health, uint16_t armour,
                   uint64_t now) {
    if (!g_cfg.anticheat || !c || c->is_npc) return;
    c->ac_stat_known = 1;
    /* Grace: abaikan laporan client sesaat setelah server mengubah nilai
     * otoritatif (hindari false positive saat client belum sync). */
    if (now - c->ac_set_ms < AC_STAT_GRACE_MS) return;
    if ((float)health > c->ac_health + AC_STAT_TOL)
        ac_report(c, "health", (double)health);
    if ((float)armour > c->ac_armour + AC_STAT_TOL)
        ac_report(c, "armour", (double)armour);
}

void ac_set_health(struct Client *c, float hp, uint64_t now) {
    if (!c) return;
    if (hp < 0) hp = 0;
    if (hp > 100) hp = 100;
    c->ac_health = hp;
    c->ac_set_ms = now;
}

void ac_set_armour(struct Client *c, float ap, uint64_t now) {
    if (!c) return;
    if (ap < 0) ap = 0;
    if (ap > 100) ap = 100;
    c->ac_armour = ap;
    c->ac_set_ms = now;
}
