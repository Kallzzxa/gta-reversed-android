// Unit test parser 0x4B SPECTATE (spectate ala SA-MP).
// Kompilasi: g++ -std=c++17 -I<repo>/android/app/src/main/cpp/src/Code/ROMP -o test_spectate_parse test_spectate_parse.cpp
// Jalankan: ./test_spectate_parse
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

// Payload 0x4B: u8 pid | u8 spectating | u8 type | u16 target LE | u8 mode.
static std::vector<uint8_t> mk(uint8_t pid, uint8_t spec, uint8_t ttype,
                               uint16_t target, uint8_t mode) {
    std::vector<uint8_t> p;
    p.push_back(pid);
    p.push_back(spec);
    p.push_back(ttype);
    p.push_back(static_cast<uint8_t>(target & 0xFF));
    p.push_back(static_cast<uint8_t>((target >> 8) & 0xFF));
    p.push_back(mode);
    return p;
}

int main() {
    // --- spectate pemain: pid=0, on, type=1, target=3, mode=1 (fixed) ---
    {
        auto p = mk(0, 1, 1, 3, 1);
        CHECK(p.size() == 6, "set: payload 6 byte");
        SpectateInfo si;
        CHECK(parse_spectate(p.data(), p.size(), si), "set: parse ok");
        CHECK(si.pid == 0, "set: pid");
        CHECK(si.spectating == 1, "set: spectating=1");
        CHECK(si.target_type == 1, "set: type=player");
        CHECK(si.target == 3, "set: target=3");
        CHECK(si.mode == 1, "set: mode=fixed");
    }
    // --- stop spectate: spectating=0, type=0, target=0, mode=0 ---
    {
        auto p = mk(5, 0, 0, 0, 0);
        SpectateInfo si;
        CHECK(parse_spectate(p.data(), p.size(), si) && si.spectating == 0 &&
              si.target_type == 0, "stop: parse ok");
    }
    // --- target kendaraan vid besar (u16 LE): vid=1200 ---
    {
        auto p = mk(2, 1, 2, 1200, 2);
        SpectateInfo si;
        CHECK(parse_spectate(p.data(), p.size(), si) && si.target == 1200 &&
              si.target_type == 2 && si.mode == 2, "vehicle: vid 1200 LE ok");
    }
    // --- payload terpotong (5 byte) -> tolak ---
    {
        auto p = mk(7, 1, 1, 9, 0);
        p.pop_back();
        SpectateInfo si;
        CHECK(!parse_spectate(p.data(), p.size(), si), "terpotong: ditolak");
    }
    // --- payload kepanjangan (7 byte) -> tolak ---
    {
        auto p = mk(7, 1, 1, 9, 0);
        p.push_back(0xAA);
        SpectateInfo si;
        CHECK(!parse_spectate(p.data(), p.size(), si), "kepanjangan: ditolak");
    }
    // --- payload kosong -> tolak ---
    {
        SpectateInfo si;
        CHECK(!parse_spectate(nullptr, 0, si), "kosong: ditolak");
    }
    // --- enum 0x4B terdaftar ---
    CHECK(static_cast<int>(PacketType::SPECTATE) == 0x4B,
          "enum SPECTATE = 0x4B");

    printf("hasil: %d PASS, %d FAIL\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
