// Unit test parser 0x43 SKILL_SET (weapon skill ala SA-MP).
// Kompilasi: g++ -std=c++17 -I<repo>/android/app/src/main/cpp/src/Code/ROMP -o test_skill_parse test_skill_parse.cpp
// Jalankan: ./test_skill_parse
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

static void tput_u16(std::vector<uint8_t>& b, uint16_t v) {
    b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF);
}

int main() {
    // --- set valid: pid=7, skill=9 (M4), level=500 ---
    {
        std::vector<uint8_t> p;
        p.push_back(7);   // pid
        p.push_back(9);   // skill M4
        tput_u16(p, 500); // level
        CHECK(p.size() == 4, "set: payload 4 byte");
        SkillSetInfo si;
        CHECK(parse_skill_set(p.data(), p.size(), si), "set: parse ok");
        CHECK(si.pid == 7, "set: pid");
        CHECK(si.skill == 9, "set: skill M4");
        CHECK(si.level == 500, "set: level 500");
    }
    // --- level batas: 0 dan 1000 ---
    {
        std::vector<uint8_t> p;
        p.push_back(1); p.push_back(0); tput_u16(p, 0);
        SkillSetInfo si;
        CHECK(parse_skill_set(p.data(), p.size(), si) && si.level == 0,
              "level 0: parse ok");
        std::vector<uint8_t> q;
        q.push_back(1); q.push_back(10); tput_u16(q, 1000);
        SkillSetInfo sj;
        CHECK(parse_skill_set(q.data(), q.size(), sj) && sj.level == 1000 &&
              sj.skill == 10, "level 1000 + skill sniper: parse ok");
    }
    // --- payload terpotong (3 byte) -> tolak ---
    {
        std::vector<uint8_t> p;
        p.push_back(7); p.push_back(9); p.push_back(0xF4); // u16 kurang 1
        SkillSetInfo si;
        CHECK(!parse_skill_set(p.data(), p.size(), si),
              "terpotong: ditolak");
    }
    // --- payload kepanjangan (5 byte) -> tolak ---
    {
        std::vector<uint8_t> p;
        p.push_back(7); p.push_back(9); tput_u16(p, 500); p.push_back(0xAA);
        SkillSetInfo si;
        CHECK(!parse_skill_set(p.data(), p.size(), si),
              "kepanjangan: ditolak");
    }
    // --- payload kosong -> tolak ---
    {
        SkillSetInfo si;
        CHECK(!parse_skill_set(nullptr, 0, si), "kosong: ditolak");
    }
    // --- enum 0x43 terdaftar ---
    CHECK(static_cast<int>(PacketType::SKILL_SET) == 0x43, "enum SKILL_SET = 0x43");

    printf("hasil: %d PASS, %d FAIL\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
