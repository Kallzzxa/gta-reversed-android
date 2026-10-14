// ============================================================
// RO-MP Protocol v1 — konstanta + helper build/parse frame
// Mirror persis dari server/PROTOCOL.md.
// Frame: [u16 len LE][u8 type][payload[len-1]]
// Header-only, tanpa dependency selain <cstdint>/<vector>/<string>.
// ============================================================
#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace ROMP {

// Versi protokol yang dikirim di HELLO.
static constexpr uint8_t kProtoVer = 1;

// Panjang frame maksimum (type + payload), sesuai PROTOCOL.md.
static constexpr uint16_t kMaxFrameLen = 4096;

// Nickname: maks 24 karakter.
static constexpr size_t kMaxNickLen = 24;

// Chat: maks 128 byte per pesan.
static constexpr size_t kMaxChatLen = 128;

// ------------------------------------------------------------
// Tipe paket
// ------------------------------------------------------------
enum class PacketType : uint8_t {
    HELLO       = 0x01,
    WELCOME     = 0x02,
    REJECT      = 0x03,
    POS         = 0x10,
    PLAYER_POS  = 0x11,
    CHAT        = 0x20,
    CHAT_MSG    = 0x21,
    PLAYER_JOIN = 0x22,
    PLAYER_LEAVE= 0x23,
    PLAYER_LIST = 0x24,
    PING        = 0x30,
    PONG        = 0x31,
    NUI_MESSAGE = 0x40,  // Server -> Client: pesan JSON untuk UI (WebView)
    NUI_CALLBACK= 0x41,  // Client -> Server: callback JS -> Lua
    NUI_FOCUS   = 0x42,  // Server -> Client: UI ambil/lepas fokus sentuhan
};

// Alasan PLAYER_LEAVE (u8 reason).
enum class LeaveReason : uint8_t {
    Quit    = 0,
    Kick    = 1,
    Timeout = 2,
};

// Kind CHAT_MSG (u8 kind).
enum class ChatKind : uint8_t {
    Normal  = 0,  // chat biasa
    Server  = 1,  // info dari server (emas)
    Me      = 2,  // /me (ungu)
    PM      = 3,  // private message (kuning)
    JoinLeave = 4,// join/leave (abu)
};

// State pemain di POS/PLAYER_POS (u8 state).
enum class PlayerState : uint8_t {
    OnFoot    = 0,
    Driver    = 1,
    Passenger = 2,
};

// ------------------------------------------------------------
// Helper little-endian (aman untuk platform apa pun).
// ------------------------------------------------------------
inline void put_u16(std::vector<uint8_t>& buf, uint16_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

inline void put_u32(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

inline void put_float(std::vector<uint8_t>& buf, float v) {
    static_assert(sizeof(float) == 4, "float harus 4 byte (IEEE-754)");
    uint32_t u;
    std::memcpy(&u, &v, sizeof(u));
    put_u32(buf, u);
}

// Cursor aman untuk membaca payload secara berurutan.
// Get* mengembalikan false jika data kurang (payload rusak).
class Reader {
public:
    explicit Reader(const uint8_t* data, size_t len) : p_(data), end_(data + len) {}

    bool get_u8(uint8_t& out) {
        if (p_ + 1 > end_) return false;
        out = *p_++;
        return true;
    }
    bool get_u16(uint16_t& out) {
        if (p_ + 2 > end_) return false;
        out = static_cast<uint16_t>(p_[0]) | (static_cast<uint16_t>(p_[1]) << 8);
        p_ += 2;
        return true;
    }
    bool get_u32(uint32_t& out) {
        if (p_ + 4 > end_) return false;
        out = static_cast<uint32_t>(p_[0])
            | (static_cast<uint32_t>(p_[1]) << 8)
            | (static_cast<uint32_t>(p_[2]) << 16)
            | (static_cast<uint32_t>(p_[3]) << 24);
        p_ += 4;
        return true;
    }
    bool get_float(float& out) {
        uint32_t u;
        if (!get_u32(u)) return false;
        std::memcpy(&out, &u, sizeof(out));
        return true;
    }
    // String UTF-8 tanpa null-terminator, panjang eksplisit.
    bool get_str(std::string& out, size_t len) {
        if (static_cast<size_t>(end_ - p_) < len) return false;
        out.assign(reinterpret_cast<const char*>(p_), len);
        p_ += len;
        return true;
    }
    // Sisa byte yang belum dibaca.
    size_t remaining() const { return static_cast<size_t>(end_ - p_); }

private:
    const uint8_t* p_;
    const uint8_t* end_;
};

// Bungkus payload jadi frame lengkap: [u16 len LE][u8 type][payload].
inline std::vector<uint8_t> make_frame(PacketType type, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> frame;
    frame.reserve(3 + payload.size());
    uint16_t len = static_cast<uint16_t>(1 + payload.size());
    put_u16(frame, len);
    frame.push_back(static_cast<uint8_t>(type));
    frame.insert(frame.end(), payload.begin(), payload.end());
    return frame;
}

// ------------------------------------------------------------
// Builder paket Client -> Server
// ------------------------------------------------------------

// 0x01 HELLO: ver(1) + namelen + name + pwlen + pw.
inline std::vector<uint8_t> build_hello(const std::string& name, const std::string& password) {
    std::string nick = name.substr(0, kMaxNickLen);
    std::vector<uint8_t> p;
    p.push_back(kProtoVer);
    p.push_back(static_cast<uint8_t>(nick.size()));
    p.insert(p.end(), nick.begin(), nick.end());
    std::string pw = password.substr(0, 255);
    p.push_back(static_cast<uint8_t>(pw.size()));
    p.insert(p.end(), pw.begin(), pw.end());
    return make_frame(PacketType::HELLO, p);
}

// 0x10 POS: x,y,z,heading (float), state(u8), vehicle_model(u16).
inline std::vector<uint8_t> build_pos(float x, float y, float z, float heading,
                                      PlayerState state, uint16_t vehicle_model) {
    std::vector<uint8_t> p;
    put_float(p, x);
    put_float(p, y);
    put_float(p, z);
    put_float(p, heading);
    p.push_back(static_cast<uint8_t>(state));
    put_u16(p, vehicle_model);
    return make_frame(PacketType::POS, p);
}

// 0x20 CHAT: len(u8) + msg.
inline std::vector<uint8_t> build_chat(const std::string& msg) {
    std::string m = msg.substr(0, kMaxChatLen);
    std::vector<uint8_t> p;
    p.push_back(static_cast<uint8_t>(m.size()));
    p.insert(p.end(), m.begin(), m.end());
    return make_frame(PacketType::CHAT, p);
}

// 0x30 PING: client_tick(u32).
inline std::vector<uint8_t> build_ping(uint32_t client_tick) {
    std::vector<uint8_t> p;
    put_u32(p, client_tick);
    return make_frame(PacketType::PING, p);
}

// ------------------------------------------------------------
// Struct hasil parse + parser paket Server -> Client
// ------------------------------------------------------------

struct WelcomeInfo {
    uint16_t playerid = 0;
    std::string server_name;
    std::string motd;
    uint8_t maxplayers = 0;
};

// 0x02 WELCOME: playerid(u16) + srvnamelen + srvname + motdlen + motd + maxplayers(u8).
inline bool parse_welcome(const uint8_t* data, size_t len, WelcomeInfo& out) {
    Reader r(data, len);
    uint8_t nlen = 0, mlen = 0;
    if (!r.get_u16(out.playerid)) return false;
    if (!r.get_u8(nlen)) return false;
    if (!r.get_str(out.server_name, nlen)) return false;
    if (!r.get_u8(mlen)) return false;
    if (!r.get_str(out.motd, mlen)) return false;
    if (!r.get_u8(out.maxplayers)) return false;
    return true;
}

struct RejectInfo {
    std::string reason;
};

// 0x03 REJECT: reasonlen(u8) + reason.
inline bool parse_reject(const uint8_t* data, size_t len, RejectInfo& out) {
    Reader r(data, len);
    uint8_t rlen = 0;
    if (!r.get_u8(rlen)) return false;
    if (!r.get_str(out.reason, rlen)) return false;
    return true;
}

struct PlayerPosInfo {
    uint16_t playerid = 0;
    float x = 0, y = 0, z = 0;
    float heading = 0;
    PlayerState state = PlayerState::OnFoot;
    uint16_t vehicle_model = 0;
};

// 0x11 PLAYER_POS: playerid(u16) + x,y,z,heading(float) + state(u8) + vehicle_model(u16).
inline bool parse_player_pos(const uint8_t* data, size_t len, PlayerPosInfo& out) {
    Reader r(data, len);
    uint8_t st = 0;
    if (!r.get_u16(out.playerid)) return false;
    if (!r.get_float(out.x)) return false;
    if (!r.get_float(out.y)) return false;
    if (!r.get_float(out.z)) return false;
    if (!r.get_float(out.heading)) return false;
    if (!r.get_u8(st)) return false;
    if (!r.get_u16(out.vehicle_model)) return false;
    out.state = static_cast<PlayerState>(st);
    return true;
}

struct ChatMsgInfo {
    uint16_t playerid = 0;  // 0xFFFF = pesan dari server
    ChatKind kind = ChatKind::Normal;
    std::string text;       // sudah diformat server
};

// 0x21 CHAT_MSG: playerid(u16) + kind(u8) + msglen(u8) + msg.
inline bool parse_chat_msg(const uint8_t* data, size_t len, ChatMsgInfo& out) {
    Reader r(data, len);
    uint8_t kind = 0, mlen = 0;
    if (!r.get_u16(out.playerid)) return false;
    if (!r.get_u8(kind)) return false;
    if (!r.get_u8(mlen)) return false;
    if (!r.get_str(out.text, mlen)) return false;
    out.kind = static_cast<ChatKind>(kind);
    return true;
}

struct PlayerJoinInfo {
    uint16_t playerid = 0;
    std::string name;
};

// 0x22 PLAYER_JOIN: playerid(u16) + namelen(u8) + name.
inline bool parse_player_join(const uint8_t* data, size_t len, PlayerJoinInfo& out) {
    Reader r(data, len);
    uint8_t nlen = 0;
    if (!r.get_u16(out.playerid)) return false;
    if (!r.get_u8(nlen)) return false;
    if (!r.get_str(out.name, nlen)) return false;
    return true;
}

struct PlayerLeaveInfo {
    uint16_t playerid = 0;
    LeaveReason reason = LeaveReason::Quit;
};

// 0x23 PLAYER_LEAVE: playerid(u16) + reason(u8).
inline bool parse_player_leave(const uint8_t* data, size_t len, PlayerLeaveInfo& out) {
    Reader r(data, len);
    uint8_t rsn = 0;
    if (!r.get_u16(out.playerid)) return false;
    if (!r.get_u8(rsn)) return false;
    out.reason = static_cast<LeaveReason>(rsn);
    return true;
}

struct PlayerListEntry {
    uint16_t playerid = 0;
    std::string name;
};

// 0x24 PLAYER_LIST: count(u8) + per pemain: playerid(u16) + namelen(u8) + name.
inline bool parse_player_list(const uint8_t* data, size_t len, std::vector<PlayerListEntry>& out) {
    Reader r(data, len);
    uint8_t count = 0;
    if (!r.get_u8(count)) return false;
    out.clear();
    out.reserve(count);
    for (uint8_t i = 0; i < count; ++i) {
        PlayerListEntry e;
        uint8_t nlen = 0;
        if (!r.get_u16(e.playerid)) return false;
        if (!r.get_u8(nlen)) return false;
        if (!r.get_str(e.name, nlen)) return false;
        out.push_back(std::move(e));
    }
    return true;
}

// 0x31 PONG: client_tick(u32).
inline bool parse_pong(const uint8_t* data, size_t len, uint32_t& out_tick) {
    Reader r(data, len);
    return r.get_u32(out_tick);
}

// 0x40 NUI_MESSAGE (S->C): jsonlen(u16) + json. Maks 4000 byte.
inline bool parse_nui_message(const uint8_t* data, size_t len, std::string& out_json) {
    Reader r(data, len);
    uint16_t jlen = 0;
    if (!r.get_u16(jlen)) return false;
    if (jlen > 4000) return false;
    return r.get_str(out_json, jlen);
}

// 0x41 NUI_CALLBACK (C->S): namelen(u8) + name + datalen(u16) + data(JSON).
inline std::vector<uint8_t> build_nui_callback(const std::string& name,
                                              const std::string& data_json) {
    std::string n = name.substr(0, 64);
    std::string d = data_json.substr(0, 4000);
    std::vector<uint8_t> p;
    p.reserve(3 + n.size() + d.size());
    p.push_back(static_cast<uint8_t>(n.size()));
    p.insert(p.end(), n.begin(), n.end());
    put_u16(p, static_cast<uint16_t>(d.size()));
    p.insert(p.end(), d.begin(), d.end());
    return make_frame(PacketType::NUI_CALLBACK, p);
}

// 0x42 NUI_FOCUS (S->C): focus(u8).
inline bool parse_nui_focus(const uint8_t* data, size_t len, bool& out_focus) {
    Reader r(data, len);
    uint8_t f = 0;
    if (!r.get_u8(f)) return false;
    out_focus = (f != 0);
    return true;
}

}  // namespace ROMP
