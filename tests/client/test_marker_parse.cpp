// Unit test parser 0x44 MARKER_SET & 0x45 MARKER_MODE (player marker ala SA-MP).
// Kompilasi: g++ -std=c++17 -I<repo>/android/app/src/main/cpp/src/Code/ROMP -o test_marker_parse test_marker_parse.cpp
// Jalankan: ./test_marker_parse
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "ROMP_Protocol.h"

using namespace ROMP;

static int g_pass = 0, g_fail = 0;

#define CHECK(cond, name) do { \
    if (cond) { g_pass++; printf("PASS %s\n", name); } \
    else { g_fail++; printf("FAIL %s\n", name); } \
} while (0)

static void tput_u32(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF);
    b.push_back((v >> 16) & 0xFF); b.push_back((v >> 24) & 0xFF);
}

int main() {
    // --- set valid: pid=7, color=0xFF0000FF (merah opaque) ---
    {
        std::vector<uint8_t> p;
        p.push_back(7);
        tput_u32(p, 0xFF0000FFu);
        CHECK(p.size() == 5, "set: payload 5 byte");
        MarkerSetInfo mi;
        CHECK(parse_marker_set(p.data(), p.size(), mi), "set: parse ok");
        CHECK(mi.pid == 7, "set: pid");
        CHECK(mi.color == 0xFF0000FFu, "set: color merah");
    }
    // --- color 0x00000000 (transparan / sembunyi ala SA-MP) ---
    {
        std::vector<uint8_t> p;
        p.push_back(1);
        tput_u32(p, 0x00000000u);
        MarkerSetInfo mi;
        CHECK(parse_marker_set(p.data(), p.size(), mi) && mi.color == 0,
              "color 0: parse ok");
    }
    // --- payload terpotong (4 byte) -> tolak ---
    {
        std::vector<uint8_t> p;
        p.push_back(7); p.push_back(0xFF); p.push_back(0x00);
        p.push_back(0x00); // u32 kurang 1
        MarkerSetInfo mi;
        CHECK(!parse_marker_set(p.data(), p.size(), mi),
              "terpotong: ditolak");
    }
    // --- payload kepanjangan (6 byte) -> tolak ---
    {
        std::vector<uint8_t> p;
        p.push_back(7); tput_u32(p, 0xFF0000FFu); p.push_back(0xAA);
        MarkerSetInfo mi;
        CHECK(!parse_marker_set(p.data(), p.size(), mi),
              "kepanjangan: ditolak");
    }
    // --- payload kosong -> tolak ---
    {
        MarkerSetInfo mi;
        CHECK(!parse_marker_set(nullptr, 0, mi), "kosong: ditolak");
    }
    // --- enum terdaftar ---
    CHECK(static_cast<int>(PacketType::MARKER_SET) == 0x44, "enum MARKER_SET = 0x44");
    CHECK(static_cast<int>(PacketType::MARKER_MODE) == 0x45, "enum MARKER_MODE = 0x45");

    // --- mode valid 0..2 ---
    for (uint8_t m = 0; m <= 2; m++) {
        uint8_t out = 99;
        char nm[32];
        snprintf(nm, sizeof(nm), "mode %u: parse ok", m);
        CHECK(parse_marker_mode(&m, 1, out) && out == m, nm);
    }
    // --- mode 3 (invalid) -> tolak ---
    {
        uint8_t bad = 3, out = 99;
        CHECK(!parse_marker_mode(&bad, 1, out), "mode 3: ditolak");
    }
    // --- mode payload kosong / kepanjangan -> tolak ---
    {
        uint8_t out = 99, m = 1;
        CHECK(!parse_marker_mode(nullptr, 0, out), "mode kosong: ditolak");
        uint8_t two[2] = {1, 2};
        CHECK(!parse_marker_mode(two, 2, out), "mode 2 byte: ditolak");
    }

    printf("hasil: %d PASS, %d FAIL\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
