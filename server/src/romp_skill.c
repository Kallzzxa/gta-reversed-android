/*
 * RO-MP weapon skill ala SA-MP (SetPlayerSkillLevel/GetPlayerSkillLevel)
 * — implementasi.
 * Lihat romp_skill.h untuk desain protokol & API.
 */
#include "romp_skill.h"

/* ---- Helper frame (duplikat kecil dari romp_server.c agar modul mandiri) ---- */
static void sput_u16(uint8_t *b, uint16_t v) {
    b[0] = v & 0xFF; b[1] = (v >> 8) & 0xFF;
}

/* Panjang frame 0x43: 1 (type) + 1 (pid) + 1 (skill) + 2 (level) = 5. */
#define SKILL_SET_LEN 5

/* Bangun frame 0x43 untuk (pid, skill, level). Kembalikan panjang. */
static size_t skill_set_frame(int pid, int skill, int level, uint8_t *b) {
    b[0] = P_SKILL_SET;
    b[1] = (uint8_t)pid;
    b[2] = (uint8_t)skill;
    sput_u16(b + 3, (uint16_t)level);
    return SKILL_SET_LEN;
}

/* ---------------- API inti ---------------- */

int skill_valid_id(int skill) {
    return skill >= 0 && skill < SKILL_COUNT;
}

int skill_valid_level(int level) {
    return level >= SKILL_LEVEL_MIN && level <= SKILL_LEVEL_MAX;
}

int skill_set(Client *c, int skill, int level) {
    if (!c || !c->handshaked || c->dead) return 0;
    if (!skill_valid_id(skill)) return 0;
    if (!skill_valid_level(level)) return 0;

    /* Simpan state otoritatif. */
    c->skill[skill] = level;

    /* Kirim 0x43 hanya ke pemain target (no-op untuk NPC — NPC tak punya
     * socket, state tetap tersimpan dan bisa dibaca gamemode). */
    uint8_t b[SKILL_SET_LEN];
    client_send_frame(c, b, skill_set_frame(c->id, skill, level, b));

    log_msg("[skill] %s (id %d): skill %d -> level %d",
            c->name, c->id, skill, level);
    return 1;
}

int skill_get(const Client *c, int skill) {
    if (!c || !c->handshaked || c->dead) return -1;
    if (!skill_valid_id(skill)) return -1;
    return c->skill[skill];
}

void skill_init(Client *c) {
    if (!c) return;
    for (int i = 0; i < SKILL_COUNT; i++)
        c->skill[i] = SKILL_DEFAULT_LEVEL;
}

void skill_on_player_leave(Client *c) {
    if (!c) return;
    /* Skill bersifat per-pemain: tak ada paket yang perlu dikirim, cukup
     * kembalikan ke default agar state tak bocor bila struct dipakai ulang. */
    skill_init(c);
}
