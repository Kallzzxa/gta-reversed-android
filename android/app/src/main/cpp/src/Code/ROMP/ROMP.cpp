// ============================================================
// ROMP — implementasi facade client.
// ============================================================

#include "ROMP.h"

#include <chrono>
#include <thread>

namespace ROMP {

namespace {
uint64_t SteadyMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}
}  // namespace

Client::Client() {
    platform_ = &null_platform_;
    start_ms_ = SteadyMs();
    last_update_ms_ = start_ms_;
}

Client& Client::Instance() {
    static Client inst;
    return inst;
}

void Client::Init(IROMPPlatform* platform) {
    if (platform) platform_ = platform;
}

uint64_t Client::NowMs() const {
    return SteadyMs() - start_ms_;
}

bool Client::Connect(const std::string& host, uint16_t port,
                     const std::string& nick, const std::string& password) {
    Disconnect();
    nick_ = nick.substr(0, kMaxNickLen);
    reject_reason_.clear();

    platform_->Log("Menghubungkan ke " + host + ":" + std::to_string(port) + " ...");
    if (!net_.Connect(host, port)) {
        platform_->Log("Gagal konek: tidak bisa mencapai server.");
        chat_.AddSystemLine("Gagal terhubung ke server.", NowMs());
        return false;
    }

    // Kirim HELLO lalu tunggu WELCOME/REJECT maks 5 detik.
    net_.SetPacketCallback(
        [this](PacketType t, const uint8_t* p, size_t n) { OnPacket(t, p, n); });
    net_.SendFrame(build_hello(nick_, password));

    hs_waiting_ = true;
    hs_ok_ = false;
    uint64_t deadline = SteadyMs() + 5000;
    while (hs_waiting_ && SteadyMs() < deadline) {
        if (!net_.Pump()) break;  // koneksi putus saat handshake
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    hs_waiting_ = false;

    if (!hs_ok_) {
        if (reject_reason_.empty())
            reject_reason_ = "Timeout: server tidak menjawab handshake.";
        platform_->Log("Handshake gagal: " + reject_reason_);
        chat_.AddSystemLine("Koneksi ditolak: " + reject_reason_, NowMs());
        net_.Close();
        return false;
    }

    state_ = ConnState::Connected;
    last_pos_ms_ = NowMs();
    last_ping_ms_ = NowMs();
    platform_->Log("Terhubung! ID pemain: " + std::to_string(local_id_));
    chat_.AddSystemLine("Terhubung ke " + server_info_.server_name +
                        " (ID " + std::to_string(local_id_) + ")", NowMs());
    if (!server_info_.motd.empty())
        chat_.AddLine(ChatKind::Server, "MOTD: " + server_info_.motd, NowMs());
    return true;
}

void Client::Disconnect() {
    if (state_ == ConnState::Connected || net_.IsConnected()) {
        chat_.AddSystemLine("Terputus dari server.", NowMs());
        platform_->Log("Disconnect dari server.");
    }
    // Hapus semua representasi remote dari dunia game.
    if (platform_) {
        for (uint16_t id : players_.GetAllIds())
            platform_->RemoveRemotePlayer(id);
    }
    players_.Clear();
    state_ = ConnState::Disconnected;
    ping_ms_ = -1;
    net_.Close();
    hs_waiting_ = false;
    hs_ok_ = false;
}

bool Client::SendChat(const std::string& text) {
    if (state_ != ConnState::Connected) return false;
    if (text.empty()) return false;
    return net_.SendFrame(build_chat(text));
}

bool Client::SendNuiCallback(const std::string& name, const std::string& dataJson) {
    if (state_ != ConnState::Connected) return false;
    if (name.empty()) return false;
    return net_.SendFrame(build_nui_callback(name, dataJson));
}

void Client::Update() {
    if (state_ != ConnState::Connected) return;

    uint64_t now = NowMs();
    float dt = static_cast<float>(now - last_update_ms_) / 1000.0f;
    last_update_ms_ = now;

    // 1. Pompa jaringan; kalau gagal -> disconnect bersih.
    if (!net_.Pump()) {
        HandleDisconnect("Koneksi ke server terputus.");
        return;
    }

    // 2. Kirim POS tiap 100 ms dari posisi pemain lokal.
    if (now - last_pos_ms_ >= 100) {
        last_pos_ms_ = now;
        SendPos();
    }

    // 3. Interpolasi remote player + dorong ke game tiap frame.
    players_.Interpolate(dt);
    for (uint16_t id : players_.GetAllIds()) {
        const RemotePlayer* p = players_.Get(id);
        if (p) platform_->MoveRemotePlayer(id, p->x, p->y, p->z, p->heading);
    }

    // 4. PING tiap 15 detik (server timeout 60 detik).
    if (now - last_ping_ms_ >= 15000) {
        last_ping_ms_ = now;
        SendPing();
    }
}

void Client::SendPos() {
    float x, y, z, heading;
    PlayerState state;
    uint16_t vehicle_model;
    platform_->GetLocalPlayerPos(x, y, z, heading, state, vehicle_model);
    net_.SendFrame(build_pos(x, y, z, heading, state, vehicle_model));
}

void Client::SendPing() {
    uint32_t tick = static_cast<uint32_t>(NowMs() & 0xFFFFFFFFu);
    net_.SendFrame(build_ping(tick));
}

void Client::HandleDisconnect(const std::string& reason) {
    platform_->Log(reason);
    Disconnect();
}

// ------------------------------------------------------------
// Handler paket masuk
// ------------------------------------------------------------
void Client::OnPacket(PacketType type, const uint8_t* payload, size_t len) {
    switch (type) {
        case PacketType::WELCOME: {
            WelcomeInfo w;
            if (parse_welcome(payload, len, w)) {
                local_id_ = w.playerid;
                server_info_ = w;
                hs_ok_ = true;
                hs_waiting_ = false;
            }
            break;
        }
        case PacketType::REJECT: {
            RejectInfo r;
            if (parse_reject(payload, len, r)) {
                reject_reason_ = r.reason;
            } else {
                reject_reason_ = "Ditolak server (alasan tak terbaca).";
            }
            hs_waiting_ = false;
            break;
        }
        case PacketType::CHAT_MSG: {
            ChatMsgInfo m;
            if (parse_chat_msg(payload, len, m))
                chat_.AddLine(m.kind, m.text, NowMs());
            break;
        }
        case PacketType::PLAYER_JOIN: {
            PlayerJoinInfo j;
            if (!parse_player_join(payload, len, j)) break;
            players_.Add(j.playerid, j.name);
            platform_->SpawnRemotePlayer(j.playerid, j.name);
            chat_.AddLine(ChatKind::JoinLeave,
                          j.name + " (ID " + std::to_string(j.playerid) +
                              ") bergabung.",
                          NowMs());
            break;
        }
        case PacketType::PLAYER_LEAVE: {
            PlayerLeaveInfo l;
            if (!parse_player_leave(payload, len, l)) break;
            const RemotePlayer* p = players_.Get(l.playerid);
            std::string name = p ? p->name : ("ID " + std::to_string(l.playerid));
            const char* alasan = "keluar";
            if (l.reason == LeaveReason::Kick) alasan = "di-kick";
            else if (l.reason == LeaveReason::Timeout) alasan = "timeout";
            players_.Remove(l.playerid);
            platform_->RemoveRemotePlayer(l.playerid);
            chat_.AddLine(ChatKind::JoinLeave,
                          name + " " + alasan + " dari server.", NowMs());
            break;
        }
        case PacketType::PLAYER_LIST: {
            std::vector<PlayerListEntry> list;
            if (!parse_player_list(payload, len, list)) break;
            for (const auto& e : list) {
                players_.Add(e.playerid, e.name);
                platform_->SpawnRemotePlayer(e.playerid, e.name);
            }
            break;
        }
        case PacketType::PLAYER_POS: {
            PlayerPosInfo pp;
            if (!parse_player_pos(payload, len, pp)) break;
            players_.UpdateTarget(pp.playerid, pp.x, pp.y, pp.z,
                                  pp.heading, pp.state, pp.vehicle_model);
            break;
        }
        case PacketType::POS_SET: {
            // Server mem-teleport pemain lokal (romp.setPlayerPos).
            PosSetInfo ps;
            if (!parse_pos_set(payload, len, ps)) break;
            platform_->TeleportLocalPlayer(ps.x, ps.y, ps.z);
            break;
        }
        case PacketType::PONG: {
            // Server meng-echo client_tick yang kita kirim di PING (0x30).
            uint32_t tick = 0;
            if (parse_pong(payload, len, tick)) {
                uint64_t now = NowMs();
                uint64_t sent = static_cast<uint64_t>(tick);
                // Guard: abaikan echo basi/aneh (tick dari masa depan).
                if (sent <= now) ping_ms_ = static_cast<int32_t>(now - sent);
            }
            break;
        }
        case PacketType::NUI_MESSAGE: {
            std::string json;
            if (parse_nui_message(payload, len, json))
                platform_->OnNuiMessage(json);
            break;
        }
        case PacketType::NUI_FOCUS: {
            bool focused = false;
            if (parse_nui_focus(payload, len, focused))
                platform_->OnNuiFocus(focused);
            break;
        }
        case PacketType::LABEL_SPAWN: {
            // Server membuat/menampilkan 3D text label (romp.create3DTextLabel).
            LabelSpawnInfo li;
            if (!parse_label_spawn(payload, len, li)) break;
            platform_->OnLabelSpawn(li);
            break;
        }
        case PacketType::LABEL_DESPAWN: {
            if (len < 2) break;
            uint16_t labelid = static_cast<uint16_t>(payload[0] |
                                                     (payload[1] << 8));
            platform_->OnLabelDespawn(labelid);
            break;
        }
        case PacketType::LABEL_TEXT: {
            // Server mengubah teks & warna label (romp.update3DTextLabelText).
            LabelTextInfo lt;
            if (!parse_label_text(payload, len, lt)) break;
            platform_->OnLabelText(lt.labelid, lt.color, lt.text);
            break;
        }
        case PacketType::LABEL_POS: {
            // Server memindahkan label statis (romp.set3DTextLabelPos).
            LabelPosInfo lp;
            if (!parse_label_pos(payload, len, lp)) break;
            platform_->OnLabelPos(lp.labelid, lp.x, lp.y, lp.z);
            break;
        }
        default:
            // Tipe tak dikenal: abaikan (forward-compat protokol).
            break;
    }
}

}  // namespace ROMP
