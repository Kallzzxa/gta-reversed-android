/*
 * RO-MP uang pemain otoritatif ala SA-MP (SetPlayerMoney / GivePlayerMoney /
 * ResetPlayerMoney / GetPlayerMoney) — implementasi.
 * Lihat romp_money.h untuk desain protokol & API.
 */
#include "romp_money.h"
#include "romp_lua.h"

/* Panjang frame 0x49: 1 (type) + 1 (pid) + 4 (s32 amount LE) = 6. */
#define MONEY_SET_LEN 6

/* Tulis u32 LE (dipakai untuk s32 amount — representasi bit sama). */
static void moput_u32(uint8_t *b, uint32_t v) {
    b[0] = (uint8_t)(v & 0xFF);
    b[1] = (uint8_t)((v >> 8) & 0xFF);
    b[2] = (uint8_t)((v >> 16) & 0xFF);
    b[3] = (uint8_t)((v >> 24) & 0xFF);
}

/* Bangun frame 0x49 untuk (pid, amount). Kembalikan panjang. */
static size_t money_set_frame(int pid, int32_t amount, uint8_t *b) {
    b[0] = P_MONEY_SET;
    b[1] = (uint8_t)pid;
    moput_u32(b + 2, (uint32_t)amount);
    return MONEY_SET_LEN;
}

/* ---------------- API inti ---------------- */

int money_valid_amount(int64_t amount) {
    return amount >= MONEY_MIN && amount <= MONEY_MAX;
}

int money_set(Client *c, int32_t amount) {
    if (!c || !c->handshaked || c->dead) return 0;
    if (!money_valid_amount(amount)) return 0;

    int32_t old = c->money;
    c->money = amount;

    /* Per-pemain ala SA-MP: uang hanya tampil di HUD milik sendiri,
     * jadi paket hanya dikirim ke pemain bersangkutan. */
    uint8_t b[MONEY_SET_LEN];
    client_send_frame(c, b, money_set_frame(c->id, amount, b));

    log_msg("[money] %s (id %d): uang %d -> %d",
            c->name, c->id, old, amount);
    lua_on_player_money_change(c->id, old, amount);
    return 1;
}

int money_give(Client *c, int64_t delta) {
    if (!c || !c->handshaked || c->dead) return 0;
    /* Anti-dupe: jenuhkan hasil ke rentang, jangan biarkan melimpah. */
    int64_t res = (int64_t)c->money + delta;
    if (res > MONEY_MAX) res = MONEY_MAX;
    if (res < MONEY_MIN) res = MONEY_MIN;
    return money_set(c, (int32_t)res);
}

int money_reset(Client *c) {
    return money_set(c, 0);
}

int money_get(const Client *c, int32_t *out) {
    if (!c || !c->handshaked || c->dead) return 0;
    if (out) *out = c->money;
    return 1;
}

void money_init(Client *c) {
    if (!c) return;
    c->money = 0;
}

void money_on_player_leave(Client *c) {
    if (!c) return;
    /* Uang bersifat per-pemain: tak ada paket yang perlu dikirim
     * (P_LEAVE sudah memberitahu semua), cukup kembalikan ke 0 agar
     * state tak bocor bila struct dipakai ulang. */
    money_init(c);
}

void money_sync_join(Client *c) {
    if (!c || !c->handshaked || c->dead) return;
    /* Hanya miliknya sendiri — uang pemain lain bersifat pribadi. */
    uint8_t b[MONEY_SET_LEN];
    client_send_frame(c, b, money_set_frame(c->id, c->money, b));
}
