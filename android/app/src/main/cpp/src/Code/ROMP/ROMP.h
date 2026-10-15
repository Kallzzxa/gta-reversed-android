// ============================================================
// ROMP — facade client RO-MP (singleton).
//
// Cara pakai:
//   ROMP::Client::Instance().Init(&myPlatform);   // atau tanpa Init -> NullPlatform
//   ROMP::Client::Instance().Connect("127.0.0.1", 7777, "Andi", "");
//   // tiap frame game:
//   ROMP::Client::Instance().Update();
//
// Modul ini 100% portable (POSIX + C++20). Semua sentuhan ke game asli
// (spawn CPed, render text, baca posisi pemain lokal) lewat IROMPPlatform.
// Cari komentar "TODO(GAME)" untuk titik integrasi ke fungsi game asli.
// ============================================================
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

#include "ROMP_Chat.h"
#include "ROMP_Net.h"
#include "ROMP_Players.h"
#include "ROMP_Protocol.h"

namespace ROMP {

// ------------------------------------------------------------
// IROMPPlatform — jembatan ke game. Implementasikan interface ini
// dengan fungsi game asli (CPed, CFont, CWorld, dsb).
// ------------------------------------------------------------
class IROMPPlatform {
public:
    virtual ~IROMPPlatform() = default;

    // Isi posisi/heading/state pemain lokal. Dipanggil tiap 100 ms.
    // TODO(GAME): baca dari CPed pemain lokal (FindPlayerPed()).
    virtual void GetLocalPlayerPos(float& x, float& y, float& z,
                                   float& heading, PlayerState& state,
                                   uint16_t& vehicle_model) = 0;

    // Spawn representasi pemain remote di dunia game.
    // TODO(GAME): buat CPed / actor via CWorld::Add + beri model CJ.
    virtual void SpawnRemotePlayer(uint16_t id, const std::string& name) = 0;

    // Hapus representasi pemain remote dari dunia game.
    // TODO(GAME): hapus CPed yang dibuat di SpawnRemotePlayer.
    virtual void RemoveRemotePlayer(uint16_t id) = 0;

    // Pindahkan pemain remote (hasil interpolasi). Dipanggil tiap frame.
    // TODO(GAME): set posisi CPed (CPlaceable::SetPosition / teleport).
    virtual void MoveRemotePlayer(uint16_t id, float x, float y, float z,
                                  float heading) = 0;

    // Teleport pemain lokal (server -> client via paket 0x27 POS_SET,
    // mis. dari romp.setPlayerPos). Default no-op; platform game
    // menimpa untuk memindahkan CPed lokal.
    virtual void TeleportLocalPlayer(float x, float y, float z) {}

    // Log ke konsol game / logcat.
    virtual void Log(const std::string& msg) = 0;

    // Teruskan pesan NUI (JSON) dari server ke UI (WebView).
    // Implementasi nyata: panggil JS onRomMessage(json) di WebView.
    virtual void OnNuiMessage(const std::string& json) = 0;

    // UI minta fokus sentuhan (mirip SetNuiFocus di FiveM).
    virtual void OnNuiFocus(bool focused) = 0;
};

// ------------------------------------------------------------
// NullPlatform — implementasi no-op untuk test standalone.
// Semua aksi hanya dicetak ke stdout supaya modul bisa jalan
// tanpa game (dipakai test harness di tests/client/).
// ------------------------------------------------------------
class NullPlatform : public IROMPPlatform {
public:
    void GetLocalPlayerPos(float& x, float& y, float& z, float& heading,
                           PlayerState& state, uint16_t& vehicle_model) override {
        // Pemain lokal "pura-pura" jalan melingkar supaya POS berubah.
        x = 100.0f; y = 200.0f; z = 10.0f;
        heading = 90.0f;
        state = PlayerState::OnFoot;
        vehicle_model = 0;
    }
    void SpawnRemotePlayer(uint16_t id, const std::string& name) override {
        std::printf("[NullPlatform] SpawnRemotePlayer id=%u name=%s\n",
                    id, name.c_str());
    }
    void RemoveRemotePlayer(uint16_t id) override {
        std::printf("[NullPlatform] RemoveRemotePlayer id=%u\n", id);
    }
    void MoveRemotePlayer(uint16_t id, float x, float y, float z,
                          float heading) override {
        (void)id; (void)x; (void)y; (void)z; (void)heading;
        // Terlalu berisik untuk dicetak tiap frame; diam saja.
    }
    void Log(const std::string& msg) override {
        std::printf("[ROMP] %s\n", msg.c_str());
    }
    void OnNuiMessage(const std::string& json) override {
        std::printf("[NullPlatform] NUI message: %s\n", json.c_str());
        last_nui_ = json;  // untuk assert di test harness
    }
    void OnNuiFocus(bool focused) override {
        std::printf("[NullPlatform] NUI focus: %s\n",
                    focused ? "true" : "false");
        last_focus_ = focused;
    }
    std::string last_nui_;
    bool last_focus_ = false;
};

enum class ConnState {
    Disconnected,
    Connected,
};

class Client {
public:
    static Client& Instance();

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    // Set platform game. Tidak mengambil alih ownership (pointer mentah).
    // Jika tidak dipanggil, dipakai NullPlatform internal.
    void Init(IROMPPlatform* platform);

    // Connect + handshake HELLO, tunggu WELCOME/REJECT maks 5 detik.
    // Mengembalikan true jika WELCOME diterima (resmi join).
    bool Connect(const std::string& host, uint16_t port,
                 const std::string& nick, const std::string& password = "");

    void Disconnect();
    bool IsConnected() const { return state_ == ConnState::Connected; }

    // Kirim pesan chat ke server (maks 128 byte, dipotong otomatis).
    // Mengembalikan false jika tidak tersambung.
    bool SendChat(const std::string& text);

    // Kirim callback NUI (JS -> Lua) ke server. Dipanggil platform saat
    // JavaScript memanggil window.romp.postMessage(name, dataJson).
    bool SendNuiCallback(const std::string& name, const std::string& dataJson);

    // Dipanggil TIAP FRAME game. Menangani: Pump net, kirim POS tiap
    // 100 ms, interpolasi + MoveRemotePlayer, PING tiap 15 detik.
    void Update();

    // Akses modul (untuk render chat UI / daftar pemain).
    ROMPChat& Chat() { return chat_; }
    ROMPPlayers& Players() { return players_; }
    const ROMPChat& Chat() const { return chat_; }
    const ROMPPlayers& Players() const { return players_; }

    uint16_t LocalPlayerId() const { return local_id_; }
    const std::string& LocalNick() const { return nick_; }
    // RTT (ms) dari PONG terakhir; -1 = belum terukur.
    int32_t GetPingMs() const { return ping_ms_; }
    const WelcomeInfo& ServerInfo() const { return server_info_; }
    const std::string& LastRejectReason() const { return reject_reason_; }

    // Waktu monotonik (ms) sejak Client dibuat — dipakai timestamp chat.
    uint64_t NowMs() const;

private:
    Client();
    ~Client() = default;

    void OnPacket(PacketType type, const uint8_t* payload, size_t len);
    void HandleDisconnect(const std::string& reason);
    void SendPos();
    void SendPing();

    IROMPPlatform* platform_ = nullptr;
    NullPlatform null_platform_;  // default jika Init() tak dipanggil

    ROMPNet net_;
    ROMPChat chat_;
    ROMPPlayers players_;

    ConnState state_ = ConnState::Disconnected;
    std::string nick_;
    uint16_t local_id_ = 0;
    WelcomeInfo server_info_;
    std::string reject_reason_;
    int32_t ping_ms_ = -1;  // RTT dari PONG terakhir (server echo client_tick)

    // Flag handshake Connect(): menunggu WELCOME/REJECT.
    bool hs_waiting_ = false;
    bool hs_ok_ = false;

    uint64_t last_pos_ms_ = 0;
    uint64_t last_ping_ms_ = 0;
    uint64_t last_update_ms_ = 0;
    uint64_t start_ms_ = 0;
};

}  // namespace ROMP
