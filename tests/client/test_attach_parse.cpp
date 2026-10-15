// Unit test parser 0x3F ATTACH_OBJ (attached object ala SA-MP).
// Kompilasi: g++ -std=c++17 -I<repo>/android/app/src/main/cpp/src/Code/ROMP -o test_attach_parse test_attach_parse.cpp
// Jalankan: ./test_attach_parse
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
static void tput_f32(std::vector<uint8_t>& b, float f) {
    uint32_t u; memcpy(&u, &f, 4);
    tput_u32(b, u);
}

int main() {
    // --- set valid (44 byte) ---
    {
        std::vector<uint8_t> p;
        p.push_back(7);   // pid
        p.push_back(3);   // index
        tput_u32(p, 18632); // modelid
        p.push_back(6);   // bone (r hand)
        tput_f32(p, 0.1f); tput_f32(p, 0.2f); tput_f32(p, 0.3f);
        tput_f32(p, 0.0f); tput_f32(p, 0.0f); tput_f32(p, 90.0f);
        tput_f32(p, 1.0f); tput_f32(p, 1.0f); tput_f32(p, 1.0f);
        CHECK(p.size() == 43, "set: payload 43 byte");
        AttachObjInfo ai;
        CHECK(parse_attach_obj(p.data(), p.size(), ai), "set: parse ok");
        CHECK(ai.pid == 7, "set: pid");
        CHECK(ai.index == 3, "set: index");
        CHECK(ai.modelid == 18632u, "set: modelid");
        CHECK(ai.bone == 6, "set: bone");
        CHECK(fabs(ai.off[0] - 0.1f) < 1e-6 && fabs(ai.off[1] - 0.2f) < 1e-6 &&
              fabs(ai.off[2] - 0.3f) < 1e-6, "set: offset");
        CHECK(fabs(ai.rot[2] - 90.0f) < 1e-6, "set: rotasi");
        CHECK(fabs(ai.scale[0] - 1.0f) < 1e-6, "set: skala");
        CHECK(ai.modelid != 0, "set: bukan remove");
    }
    // --- remove valid (7 byte) ---
    {
        std::vector<uint8_t> p;
        p.push_back(7);   // pid
        p.push_back(3);   // index
        tput_u32(p, 0);    // modelid = 0
        AttachObjInfo ai;
        CHECK(parse_attach_obj(p.data(), p.size(), ai), "remove: parse ok");
        CHECK(ai.pid == 7 && ai.index == 3 && ai.modelid == 0,
              "remove: pid/index/modelid=0");
    }
    // --- remove palsu: 7 byte tapi modelid != 0 -> tolak ---
    {
        std::vector<uint8_t> p;
        p.push_back(7); p.push_back(3);
        tput_u32(p, 18632);
        AttachObjInfo ai;
        CHECK(!parse_attach_obj(p.data(), p.size(), ai),
              "remove palsu: ditolak");
    }
    // --- payload terpotong -> tolak ---
    {
        std::vector<uint8_t> p;
        p.push_back(7); p.push_back(3); tput_u32(p, 18632); p.push_back(6);
        tput_f32(p, 0.1f);  // cuma 1 float, kurang 8
        AttachObjInfo ai;
        CHECK(!parse_attach_obj(p.data(), p.size(), ai),
              "terpotong: ditolak");
    }
    // --- payload kosong -> tolak ---
    {
        AttachObjInfo ai;
        CHECK(!parse_attach_obj(nullptr, 0, ai), "kosong: ditolak");
    }
    // --- enum 0x3F terdaftar ---
    CHECK(static_cast<int>(PacketType::ATTACH_OBJ) == 0x3F, "enum ATTACH_OBJ = 0x3F");

    printf("hasil: %d PASS, %d FAIL\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
