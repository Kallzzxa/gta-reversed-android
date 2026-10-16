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

    // 3D text label ala SA-MP (paket 0x28-0x2B, dari romp.create3DTextLabel).
    // Default no-op; platform game menimpa untuk render label di dunia.
    // TODO(GAME): render teks 3D (billboard) di posisi / mengikuti entity.
    virtual void OnLabelSpawn(const LabelSpawnInfo& /*info*/) {}
    virtual void OnLabelDespawn(uint16_t /*labelid*/) {}
    virtual void OnLabelText(uint16_t /*labelid*/, uint32_t /*color*/,
                             const std::string& /*text*/) {}
    virtual void OnLabelPos(uint16_t /*labelid*/, float /*x*/, float /*y*/,
                            float /*z*/) {}

    // Objek ala SA-MP (paket 0x2C-0x2F, dari romp.createObject).
    // Default no-op; platform game menimpa untuk spawn/render objek di dunia.
    // TODO(GAME): spawn CObject via CStreaming/pool game dari modelid.
    virtual void OnObjectSpawn(const ObjectSpawnInfo& /*info*/) {}
    virtual void OnObjectDespawn(uint16_t /*objid*/) {}
    virtual void OnObjectMove(const ObjectMoveInfo& /*info*/) {}
    virtual void OnObjectUpdate(const ObjectUpdateInfo& /*info*/) {}

    // Actor statis ala SA-MP (paket 0x32-0x36, dari romp.createActor).
    // Default no-op; platform game menimpa untuk spawn/render ped di dunia.
    // TODO(GAME): spawn CPed via CWorld/CStreaming pool game dari modelid.
    virtual void OnActorSpawn(const ActorSpawnInfo& /*info*/) {}
    virtual void OnActorDespawn(uint16_t /*actorid*/) {}
    virtual void OnActorPos(const ActorPosInfo& /*info*/) {}
    virtual void OnActorAnim(uint16_t /*actorid*/,
                             const std::string& /*anim*/) {}
    virtual void OnActorHealth(uint16_t /*actorid*/, uint16_t /*health*/) {}

    // Audio stream ala SA-MP (paket 0x37-0x38, dari romp.playAudioStreamForPlayer).
    // Default no-op; platform game menimpa untuk memutar audio.
    // TODO(GAME): putar URL (mp3/ogg) via WebView (HTML5 <audio>) atau
    //   MediaPlayer; usepos=true -> atur volume/pan dari posisi sumber.
    virtual void OnAudioPlay(const AudioPlayInfo& /*info*/) {}
    virtual void OnAudioStop() {}

    // World settings ala SA-MP (paket 0x39, dari romp.setGravity/dsb).
    // Default no-op; platform game menimpa untuk menerapkan ke dunia.
    // TODO(GAME): terapkan ke engine GTA:SA (SET_GRAVITY via script opcode,
    //   force weather, set waktu via clock). Versi reversal masih stub.
    virtual void OnWorldSettings(const WorldSetInfo& /*info*/) {}

    // Komponen kendaraan ala SA-MP (paket 0x3A-0x3C,
    // dari romp.addVehicleComponent/changeVehiclePaintjob).
    // Default no-op; platform game menimpa untuk render modifikasi di dunia.
    // TODO(GAME): pasang/lepas carmod via pool kendaraan game (CARMOD_*).
    virtual void OnVehicleComponent(uint16_t /*vid*/, uint16_t /*compid*/,
                                    uint8_t /*slot*/) {}
    virtual void OnVehicleComponentRemove(uint16_t /*vid*/,
                                          uint16_t /*compid*/) {}
    virtual void OnVehiclePaintjob(uint16_t /*vid*/, uint8_t /*paintjob*/) {}

    // Params kendaraan ala SA-MP (paket 0x3D/0x3E,
    // dari romp.setVehicleParamsEx/setVehicleParamsForPlayer).
    // Default no-op; platform game menimpa untuk terapkan ke dunia.
    // TODO(GAME): engine/lights/doors via pool kendaraan game.
    virtual void OnVehicleParams(uint16_t /*vid*/, int8_t /*engine*/,
                                 int8_t /*lights*/, int8_t /*alarm*/,
                                 int8_t /*doors*/, int8_t /*bonnet*/,
                                 int8_t /*boot*/, int8_t /*objective*/) {}
    virtual void OnVehicleParamsForPlayer(uint16_t /*vid*/, uint16_t /*pid*/,
                                          int8_t /*objective*/,
                                          int8_t /*doorslocked*/) {}

    // Attached object ala SA-MP (paket 0x3F,
    // dari romp.setPlayerAttachedObject/removePlayerAttachedObject).
    // Default no-op; platform game menimpa untuk tempelkan objek ke bone ped.
    // TODO(GAME): buat objek game dari modelid, attach ke bone ped pemilik
    //   (CEntity::AttachToRwObject / frame bone GTA:SA), terapkan
    //   offset/rotasi/skala; OnDetachObject melepas & menghapusnya.
    virtual void OnAttachObject(const AttachObjInfo& /*info*/) {}
    virtual void OnDetachObject(uint8_t /*pid*/, uint8_t /*index*/) {}

    // Weapon skill ala SA-MP (paket 0x43, dari romp.setPlayerSkillLevel).
    // Dikirim hanya ke pemain yang skill-nya berubah. Default no-op;
    // platform game menimpa untuk menerapkan level ke handling senjata.
    // TODO(GAME): terapkan ke stat senjata GTA:SA (CStats /
    //   skill-dependent weapon handling, 0-1000).
    virtual void OnPlayerSkill(uint8_t /*pid*/, uint8_t /*skill*/,
                               uint16_t /*level*/) {}

    // Player marker ala SA-MP (paket 0x44/0x45, dari
    // romp.setPlayerColor/setPlayerMarkerForPlayer/showPlayerMarkers).
    // OnPlayerMarker: warna marker efektif pemain pid menurut pandangan
    // client ini (0xRRGGBBAA). OnPlayerMarkerMode: mode global
    // (0=off, 1=global, 2=streamed). Default no-op.
    // TODO(GAME): gambar blip berwarna di radar/minimap GTA:SA untuk
    //   tiap pemain remote; mode 0 = sembunyikan semua blip pemain.
    virtual void OnPlayerMarker(uint8_t /*pid*/, uint32_t /*color*/) {}
    virtual void OnPlayerMarkerMode(uint8_t /*mode*/) {}

    // Kill feed ala SA-MP (paket 0x47 DEATH_MSG, dari laporan client 0x46
    // atau romp.sendDeathMessage). killerid 255 = tak ada pembunuh,
    // reason = id senjata ala SA-MP (1..46) / kode khusus (49..54).
    // Default no-op.
    // TODO(GAME): tampilkan baris kill feed ala SA-MP di pojok kanan atas
    //   ("pembunuh [senjata] korban") saat pemain lokal/mana pun mati.
    virtual void OnDeathMessage(uint8_t /*killerid*/, uint8_t /*victimid*/,
                                uint8_t /*weaponid*/) {}

    // Wanted level ala SA-MP (paket 0x48 WANTED_SET, dari
    // romp.setPlayerWantedLevel): level wanted pemain pid berubah menjadi
    // level (0=bersih .. 6=paling dicari). Broadcast global — client
    // menerima wanted semua pemain. Default no-op.
    // TODO(GAME): tampilkan bintang wanted (0-6) ala HUD GTA:SA untuk
    //   tiap pemain remote; trigger skin/mobil polisi saat level >= 3.
    virtual void OnPlayerWanted(uint8_t /*pid*/, uint8_t /*level*/) {}

    // Tim pemain ala SA-MP (paket 0x53 TEAM_SET, dari
    // romp.setPlayerTeam): tim pemain pid berubah menjadi team
    // (0..254 = id tim, 255 = TEAM_NONE tanpa tim ala NO_TEAM).
    // Broadcast global — client menerima tim semua pemain. Default no-op.
    // TODO(GAME): warnai nametag/scoreboard/marker pemain remote sesuai
    //   tim (mis. tim 1 = biru, tim 2 = merah) untuk gangwar/dm.
    virtual void OnPlayerTeam(uint8_t /*pid*/, uint8_t /*team*/) {}

    // Uang pemain otoritatif ala SA-MP (paket 0x49 MONEY_SET, dari
    // romp.setPlayerMoney/givePlayerMoney/resetPlayerMoney): saldo pemain
    // pid berubah menjadi amount. Per-pemain — client hanya menerima
    // saldo miliknya sendiri. Default no-op.
    // TODO(GAME): tampilkan saldo di HUD ala GTA:SA.
    virtual void OnPlayerMoney(uint8_t /*pid*/, int32_t /*amount*/) {}

    // Plat nomor kendaraan ala SA-MP (paket 0x4A VEH_PLATE, dari
    // romp.setVehicleNumberPlate): teks plat kendaraan vid berubah menjadi
    // plate ("" = plat dihapus). Broadcast vw-scoped + resync saat join /
    // pindah vw. Default no-op.
    // TODO(GAME): tampilkan teks plat pada model kendaraan.
    virtual void OnVehicleNumberPlate(uint16_t /*vid*/,
                                      const std::string& /*plate*/) {}

    // Spectate ala SA-MP (paket 0x4B SPECTATE, dari
    // romp.togglePlayerSpectating/playerSpectatePlayer/
    // playerSpectateVehicle): pemain pid masuk (spectating=1) atau keluar
    // (spectating=0) mode penonton. target_type 0=none/1=pemain/2=kendaraan,
    // target = pid/vid (0 bila none), mode 0=normal/1=fixed/2=side.
    // Broadcast vw-scoped ke vw penonton + resync saat join / pindah vw.
    // Bila pid adalah pemain lokal: alihkan kamera mengikuti target.
    // Bila pemain lain: sembunyikan (spectating=1) / tampilkan lagi
    // (spectating=0) ped-nya. Default no-op.
    // TODO(GAME): ikuti target via kamera; hide/show ped remote.
    virtual void OnPlayerSpectate(uint8_t /*pid*/, uint8_t /*spectating*/,
                                  uint8_t /*target_type*/, uint16_t /*target*/,
                                  uint8_t /*mode*/) {}

    // Key state ala SA-MP (paket 0x4D KEY_SYNC, dari
    // romp.getPlayerKeys/event onPlayerKeyStateChange): bitmask tombol
    // pemain pid berubah (KEY_ACTION=1, KEY_FIRE=4, KEY_JUMP=32, dst —
    // hanya 16 bit terendah yang terkirim). Broadcast vw-scoped ke viewer
    // yang melihat pemain itu + resync saat join / pindah vw.
    // Default no-op.
    // TODO(GAME): visualisasikan key state ped remote (mis. pose
    //   nembak/jongkok dari bit KEY_FIRE/KEY_CROUCH; chat bubble sprint).
    virtual void OnPlayerKeys(uint8_t /*pid*/, uint16_t /*keys*/) {}

    // Animasi pemain ala SA-MP (paket 0x4E ANIM_SET / 0x4F ANIM_CLEAR,
    // dari romp.applyAnimationForPlayer/clearAnimationsForPlayer):
    // pemain pid mulai (animlib/animname, mis. "DANCING"/"dnce_m_a",
    // flags bit0=loop/bit1=lockx/bit2=locky/bit3=freeze/bit4=forcesync,
    // timems durasi ms) atau berhenti beranimasi. Broadcast vw-scoped +
    // resync saat join / pindah vw. Default no-op.
    // TODO(GAME): putar animasi di ped remote via GTA anim lib
    //   (mis. CAnimManager::BlendAnimation) bila reversal tersedia.
    virtual void OnPlayerAnimation(uint8_t /*pid*/,
                                   const std::string& /*animlib*/,
                                   const std::string& /*animname*/,
                                   uint8_t /*flags*/,
                                   uint16_t /*timems*/) {}
    virtual void OnPlayerClearAnimations(uint8_t /*pid*/) {}

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

    // Lapor kematian pemain lokal ke server (paket 0x46 DEATH_REPORT).
    // killerid 255 = tak ada pembunuh; reason = id senjata / kode khusus.
    // Server memvalidasi lalu broadcast kill feed 0x47.
    // Dipanggil platform saat ped pemain lokal mati di game.
    // TODO(GAME): hubungkan ke hook kematian ped GTA:SA.
    bool SendDeathReport(uint8_t killerid, uint8_t reason);

    // Kirim key state pemain lokal ke server (paket 0x4C KEY_STATE).
    // keys = bitmask tombol ala SA-MP (hanya 16 bit terendah yang
    // terkirim, wire u16 LE). Kirim tiap state tombol berubah — bukan
    // tiap frame.
    // Dipanggil platform saat input pemain lokal berubah di game.
    // TODO(GAME): hubungkan ke pembacaan input pad/game ala GTA:SA.
    bool SendKeyState(uint32_t keys);

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
