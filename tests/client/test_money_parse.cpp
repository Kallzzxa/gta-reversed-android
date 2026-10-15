// Unit test parser 0x49 MONEY_SET (uang pemain otoritatif ala SA-MP).
// Kompilasi: g++ -std=c++17 -I<repo>/android/app/src/main/cpp/src/Code/ROMP -o test_money_parse test_money_parse.cpp
// Jalankan: ./test_money_parse
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

#include "ROMP_Protocol.h"

using namespace ROMP;

static int g_pass = 0, g_fail = 0;

#define CHECK(cond, name) do { \
    if (cond) { g_pass++; printf("PASS %s\n", name); } \
    else { g_fail++; printf("FAIL %s\n", name); } \
} while (0)

// Payload 0x49: u8 pid + s32 amount LE.
static std::vector<uint8_t> mk(uint8_t pid, int32_t amount) {
    std::vector<uint8_t> p;
    p.push_back(pid);
    uint32_t u = static_cast<uint32_t>(amount);
    p.push_back(static_cast<uint8_t>(u & 0xFF));
    p.push_back(static_cast<uint8_t>((u >> 8) & 0xFF));
    p.push_back(static_cast<uint8_t>((u >> 16) & 0xFF));
    p.push_back(static_cast<uint8_t>((u >> 24) & 0xFF));
    return p;
}

int main() {
    // --- set valid: pid=3, amount=5000 ---
    {
        auto p = mk(3, 5000);
        CHECK(p.size() == 5, "set: payload 5 byte");
        MoneySetInfo mi;
        CHECK(parse_money_set(p.data(), p.size(), mi), "set: parse ok");
        CHECK(mi.pid == 3, "set: pid");
        CHECK(mi.amount == 5000, "set: amount 5000");
    }
    // --- amount negatif: -500 ---
    {
        auto p = mk(1, -500);
        MoneySetInfo mi;
        CHECK(parse_money_set(p.data(), p.size(), mi) && mi.amount == -500,
              "negatif: -500 parse ok");
    }
    // --- batas: 999999999 dan -999999999 ---
    {
        auto p = mk(1, 999999999);
        MoneySetInfo mi;
        CHECK(parse_money_set(p.data(), p.size(), mi) && mi.amount == 999999999,
              "maks: 999999999");
        auto q = mk(1, -999999999);
        MoneySetInfo mj;
        CHECK(parse_money_set(q.data(), q.size(), mj) && mj.amount == -999999999,
              "min: -999999999");
    }
    // --- payload terpotong (4 byte) -> tolak ---
    {
        auto p = mk(7, 100);
        p.pop_back();
        MoneySetInfo mi;
        CHECK(!parse_money_set(p.data(), p.size(), mi),
              "terpotong: ditolak");
    }
    // --- payload kepanjangan (6 byte) -> tolak ---
    {
        auto p = mk(7, 100);
        p.push_back(0xAA);
        MoneySetInfo mi;
        CHECK(!parse_money_set(p.data(), p.size(), mi),
              "kepanjangan: ditolak");
    }
    // --- payload kosong -> tolak ---
    {
        MoneySetInfo mi;
        CHECK(!parse_money_set(nullptr, 0, mi), "kosong: ditolak");
    }
    // --- enum 0x49 terdaftar ---
    CHECK(static_cast<int>(PacketType::MONEY_SET) == 0x49,
          "enum MONEY_SET = 0x49");

    printf("hasil: %d PASS, %d FAIL\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
