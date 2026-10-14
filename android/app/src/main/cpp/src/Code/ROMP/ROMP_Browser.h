// ============================================================
// ROMP_Browser — server browser (portable POSIX/C++20).
//
// Dua sumber daftar server:
//   1. Master server  : TCP, daftar server yang announce (lihat
//                       server/src/romp_master.c). Cocok untuk browser
//                       publik ("server apa saja yang nyala?").
//   2. Query langsung : UDP ke IP:port server (protokol query di
//                       PROTOCOL.md). Untuk info live + daftar pemain.
//
// Semua method sinkron dengan timeout — panggil dari background thread,
// JANGAN dari UI thread.
//
// Contoh:
//   ROMP::BrowserServer info;
//   if (ROMP::ROMPBrowser::QueryServer("127.0.0.1", 7777, info))
//       printf("%s (%u/%u) ping %d ms\n", info.hostname.c_str(),
//              info.players, info.maxplayers, info.ping_ms);
//
//   std::vector<ROMP::BrowserServer> list;
//   if (ROMP::ROMPBrowser::FetchMasterList("127.0.0.1", 7778, list))
//       for (auto& s : list) ROMP::ROMPBrowser::QueryServer(s.ip, s.port, s);
// ============================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ROMP {

struct BrowserPlayer {
    uint16_t id = 0;
    std::string name;
};

struct BrowserServer {
    std::string ip;
    uint16_t port = 0;
    std::string hostname;
    std::string motd;
    std::string version;
    uint16_t players = 0;
    uint16_t maxplayers = 0;
    bool has_password = false;
    std::vector<BrowserPlayer> player_list;
    int ping_ms = -1;  // RTT query info; -1 = tak menjawab
};

class ROMPBrowser {
public:
    ROMPBrowser() = delete;

    // Query satu server via UDP (info + daftar pemain).
    // Mengisi `out` (ip/port di-set dari argumen). true jika server menjawab.
    static bool QueryServer(const std::string& host, uint16_t port,
                            BrowserServer& out, int timeout_ms = 1500);

    // Ambil daftar server dari master via TCP.
    // Entri hanya berisi ip/port/hostname/motd/players/maxplayers apa adanya
    // dari announce terakhir — panggil QueryServer per entri untuk info live.
    static bool FetchMasterList(const std::string& master_host,
                                uint16_t master_port,
                                std::vector<BrowserServer>& out,
                                int timeout_ms = 4000);
};

}  // namespace ROMP
