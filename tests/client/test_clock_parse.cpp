// ============================================================
// Test unit parse clock toggle RO-MP (0x54 CLOCK_SET).
//
// Header-only ROMP_Protocol.h — tanpa butuh server, NDK, atau game.
// Gaya check PASS/FAIL meniru test_main.cpp.
//
// Kompilasi: lihat run_test.sh (bagian "Unit test parse clock").
// ============================================================

#include <cstdint>
#include <cstdio>
#include <vector>

#include "Code/ROMP/ROMP_Protocol.h"

using namespace ROMP;

static int g_failures = 0;

static void check(bool ok, const char* name) {
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
    std::fflush(stdout);
    if (!ok) ++g_failures;
}

int main() {
    // ---- 1. Parse valid: pid=5, enabled=1 (jam berjalan) ----
    {
        const uint8_t payload[] = {5, 1};
        ClockSetInfo ci;
        bool ok = parse_clock_set(payload, sizeof(payload), ci);
        check(ok && ci.pid == 5 && ci.enabled == true,
              "clock_parse_valid_enabled");
    }

    // ---- 2. Parse valid: pid=0, enabled=0 (dibekukan) ----
    {
        const uint8_t payload[] = {0, 0};
        ClockSetInfo ci;
        bool ok = parse_clock_set(payload, sizeof(payload), ci);
        check(ok && ci.pid == 0 && ci.enabled == false,
              "clock_parse_valid_disabled");
    }

    // ---- 3. Nilai selain 0/1 dianggap enabled (truthy) ----
    {
        const uint8_t payload[] = {7, 0xFF};
        ClockSetInfo ci;
        bool ok = parse_clock_set(payload, sizeof(payload), ci);
        check(ok && ci.pid == 7 && ci.enabled == true,
              "clock_parse_nonzero_is_enabled");
    }

    // ---- 4. Panjang salah (1 byte) -> false ----
    {
        const uint8_t payload[] = {5};
        ClockSetInfo ci;
        check(!parse_clock_set(payload, sizeof(payload), ci),
              "clock_parse_len1_reject");
    }

    // ---- 5. Panjang salah (3 byte) -> false ----
    {
        const uint8_t payload[] = {5, 1, 0xAA};
        ClockSetInfo ci;
        check(!parse_clock_set(payload, sizeof(payload), ci),
              "clock_parse_len3_reject");
    }

    // ---- 6. Enum bernilai 0x54 ----
    {
        check(static_cast<uint8_t>(PacketType::CLOCK_SET) == 0x54,
              "clock_enum_is_0x54");
    }

    // ---- 7. Roundtrip: frame 0x54 -> payload -> parse ----
    {
        // frame = [len LE=3][type=0x54][pid][enabled]
        std::vector<uint8_t> frame = {0x03, 0x00,
                                      static_cast<uint8_t>(PacketType::CLOCK_SET),
                                      12, 0};
        const uint8_t* payload = frame.data() + 3;
        ClockSetInfo ci;
        bool ok = parse_clock_set(payload, 2, ci);
        check(ok && ci.pid == 12 && ci.enabled == false,
              "clock_frame_roundtrip_disabled");
    }

    if (g_failures == 0) {
        std::printf("SEMUA TEST CLOCK LOLOS\n");
    } else {
        std::printf("GAGAL: %d cek\n", g_failures);
    }
    return g_failures == 0 ? 0 : 1;
}
