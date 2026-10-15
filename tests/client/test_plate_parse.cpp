// Unit test parser 0x4A VEH_PLATE (plat nomor kendaraan ala SA-MP).
// Kompilasi: g++ -std=c++17 -I<repo>/android/app/src/main/cpp/src/Code/ROMP -o test_plate_parse test_plate_parse.cpp
// Jalankan: ./test_plate_parse
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

// Payload 0x4A: u16 vid LE + u8 len + bytes(len).
static std::vector<uint8_t> mk(uint16_t vid, const char* plate) {
    std::vector<uint8_t> p;
    p.push_back(static_cast<uint8_t>(vid & 0xFF));
    p.push_back(static_cast<uint8_t>((vid >> 8) & 0xFF));
    size_t n = strlen(plate);
    p.push_back(static_cast<uint8_t>(n));
    for (size_t i = 0; i < n; i++) p.push_back(static_cast<uint8_t>(plate[i]));
    return p;
}

int main() {
    // --- set valid: vid=1, plate "RO-MP-1" ---
    {
        auto p = mk(1, "RO-MP-1");
        CHECK(p.size() == 10, "set: payload 10 byte");
        PlateInfo pi;
        CHECK(parse_plate(p.data(), p.size(), pi), "set: parse ok");
        CHECK(pi.vid == 1, "set: vid 1");
        CHECK(pi.plate == "RO-MP-1", "set: plate RO-MP-1");
    }
    // --- plat kosong (dihapus): len 0 ---
    {
        auto p = mk(5, "");
        PlateInfo pi;
        CHECK(parse_plate(p.data(), p.size(), pi), "kosong: parse ok");
        CHECK(pi.vid == 5 && pi.plate.empty(), "kosong: vid + plate empty");
    }
    // --- plat maks 32 char ---
    {
        std::string s(32, 'A');
        auto p = mk(2, s.c_str());
        PlateInfo pi;
        CHECK(parse_plate(p.data(), p.size(), pi) && pi.plate == s,
              "maks 32 char: parse ok");
        CHECK(p.size() == 35, "maks 32 char: payload 35 byte");
    }
    // --- vid besar (u16) ---
    {
        auto p = mk(1000, "X");
        PlateInfo pi;
        CHECK(parse_plate(p.data(), p.size(), pi) && pi.vid == 1000,
              "vid 1000 parse ok");
    }
    // --- payload terpotong (len 5 tapi byte hanya 3) -> tolak ---
    {
        auto p = mk(1, "ABC");
        p.pop_back();
        PlateInfo pi;
        CHECK(!parse_plate(p.data(), p.size(), pi), "terpotong: ditolak");
    }
    // --- len 33 (melebihi maks) -> tolak ---
    {
        auto p = mk(1, "A");
        p[2] = 33;
        PlateInfo pi;
        CHECK(!parse_plate(p.data(), p.size(), pi), "len 33: ditolak");
    }
    // --- payload kepanjangan (len 2 tapi 3 byte) -> tolak ---
    {
        auto p = mk(1, "AB");
        p.push_back(0xAA);
        PlateInfo pi;
        CHECK(!parse_plate(p.data(), p.size(), pi), "kepanjangan: ditolak");
    }
    // --- payload kosong -> tolak ---
    {
        PlateInfo pi;
        CHECK(!parse_plate(nullptr, 0, pi), "kosong: ditolak");
    }
    // --- enum 0x4A terdaftar ---
    CHECK(static_cast<int>(PacketType::VEH_PLATE) == 0x4A,
          "enum VEH_PLATE = 0x4A");

    printf("hasil: %d PASS, %d FAIL\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
