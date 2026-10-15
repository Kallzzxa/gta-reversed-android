// Unit test parser 0x48 WANTED_SET (wanted level ala SA-MP).
// Kompilasi: g++ -std=c++17 -I<repo>/android/app/src/main/cpp/src/Code/ROMP -o test_wanted_parse test_wanted_parse.cpp
// Jalankan: ./test_wanted_parse
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

int main() {
    // --- set valid: pid=7, level=4 ---
    {
        std::vector<uint8_t> p;
        p.push_back(7); // pid
        p.push_back(4); // level wanted
        CHECK(p.size() == 2, "set: payload 2 byte");
        WantedSetInfo wi;
        CHECK(parse_wanted_set(p.data(), p.size(), wi), "set: parse ok");
        CHECK(wi.pid == 7, "set: pid");
        CHECK(wi.level == 4, "set: level 4");
    }
    // --- level batas: 0 (bersih) dan 6 (paling dicari) ---
    {
        std::vector<uint8_t> p;
        p.push_back(1); p.push_back(0);
        WantedSetInfo wi;
        CHECK(parse_wanted_set(p.data(), p.size(), wi) && wi.level == 0,
              "level 0: parse ok");
        std::vector<uint8_t> q;
        q.push_back(1); q.push_back(6);
        WantedSetInfo wj;
        CHECK(parse_wanted_set(q.data(), q.size(), wj) && wj.level == 6,
              "level 6: parse ok");
    }
    // --- payload terpotong (1 byte) -> tolak ---
    {
        std::vector<uint8_t> p;
        p.push_back(7);
        WantedSetInfo wi;
        CHECK(!parse_wanted_set(p.data(), p.size(), wi),
              "terpotong: ditolak");
    }
    // --- payload kepanjangan (3 byte) -> tolak ---
    {
        std::vector<uint8_t> p;
        p.push_back(7); p.push_back(4); p.push_back(0xAA);
        WantedSetInfo wi;
        CHECK(!parse_wanted_set(p.data(), p.size(), wi),
              "kepanjangan: ditolak");
    }
    // --- payload kosong -> tolak ---
    {
        WantedSetInfo wi;
        CHECK(!parse_wanted_set(nullptr, 0, wi), "kosong: ditolak");
    }
    // --- enum 0x48 terdaftar ---
    CHECK(static_cast<int>(PacketType::WANTED_SET) == 0x48,
          "enum WANTED_SET = 0x48");

    printf("hasil: %d PASS, %d FAIL\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
