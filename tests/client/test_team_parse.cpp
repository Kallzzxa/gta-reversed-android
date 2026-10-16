// Unit test parser 0x53 TEAM_SET (tim pemain ala SA-MP).
// Kompilasi: g++ -std=c++17 -I<repo>/android/app/src/main/cpp/src/Code/ROMP -o test_team_parse test_team_parse.cpp
// Jalankan: ./test_team_parse
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
    // --- set valid: pid=5, team=2 ---
    {
        std::vector<uint8_t> p;
        p.push_back(5); // pid
        p.push_back(2); // team
        CHECK(p.size() == 2, "set: payload 2 byte");
        TeamSetInfo ti;
        CHECK(parse_team_set(p.data(), p.size(), ti), "set: parse ok");
        CHECK(ti.pid == 5, "set: pid");
        CHECK(ti.team == 2, "set: team 2");
    }
    // --- batas: team 0 (tim pertama) dan 255 (TEAM_NONE) ---
    {
        std::vector<uint8_t> p;
        p.push_back(1); p.push_back(0);
        TeamSetInfo ti;
        CHECK(parse_team_set(p.data(), p.size(), ti) && ti.team == 0,
              "team 0: parse ok");
        std::vector<uint8_t> q;
        q.push_back(1); q.push_back(255);
        TeamSetInfo tj;
        CHECK(parse_team_set(q.data(), q.size(), tj) && tj.team == 255,
              "team 255 (TEAM_NONE): parse ok");
    }
    // --- payload terpotong (1 byte) -> tolak ---
    {
        std::vector<uint8_t> p;
        p.push_back(5);
        TeamSetInfo ti;
        CHECK(!parse_team_set(p.data(), p.size(), ti),
              "terpotong: ditolak");
    }
    // --- payload kepanjangan (3 byte) -> tolak ---
    {
        std::vector<uint8_t> p;
        p.push_back(5); p.push_back(2); p.push_back(0xAA);
        TeamSetInfo ti;
        CHECK(!parse_team_set(p.data(), p.size(), ti),
              "kepanjangan: ditolak");
    }
    // --- payload kosong -> tolak ---
    {
        TeamSetInfo ti;
        CHECK(!parse_team_set(nullptr, 0, ti), "kosong: ditolak");
    }
    // --- enum 0x53 terdaftar ---
    CHECK(static_cast<int>(PacketType::TEAM_SET) == 0x53,
          "enum TEAM_SET = 0x53");

    printf("hasil: %d PASS, %d FAIL\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
