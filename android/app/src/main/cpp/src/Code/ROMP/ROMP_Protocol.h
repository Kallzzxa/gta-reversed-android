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
    POS_SET     = 0x27,  // Server -> Client: teleport pemain lokal
                         // (payload: float x,y,z).
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
    // 3D text label ala SA-MP (Create3DTextLabel), server -> client.
    LABEL_SPAWN   = 0x28,  // label muncul
    LABEL_DESPAWN = 0x29,  // label hilang
    LABEL_TEXT    = 0x2A,  // ubah teks & warna label
    LABEL_POS     = 0x2B,  // pindahkan label statis
    // Objek ala SA-MP (CreateObject/MoveObject), server -> client.
    OBJECT_SPAWN   = 0x2C,  // objek muncul
    OBJECT_DESPAWN = 0x2D,  // objek hilang
    OBJECT_MOVE    = 0x2E,  // objek bergerak (MoveObject)
    OBJECT_UPDATE  = 0x2F,  // set posisi/rotasi instan
    // Actor statis ala SA-MP (CreateActor), server -> client.
    ACTOR_SPAWN    = 0x32,  // actor muncul
    ACTOR_DESPAWN  = 0x33,  // actor hilang
    ACTOR_POS      = 0x34,  // pindah actor (set pos+angle)
    ACTOR_ANIM     = 0x35,  // terapkan animasi actor
    ACTOR_HEALTH   = 0x36,  // health actor berubah
    // Audio stream ala SA-MP (PlayAudioStreamForPlayer), server -> client.
    AUDIO_PLAY     = 0x37,  // putar URL audio
    AUDIO_STOP     = 0x38,  // hentikan audio yang diputar
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

struct PosSetInfo {
    float x = 0, y = 0, z = 0;
};

// 0x27 POS_SET: teleport pemain lokal. x,y,z(float).
inline bool parse_pos_set(const uint8_t* data, size_t len, PosSetInfo& out) {
    Reader r(data, len);
    if (!r.get_float(out.x)) return false;
    if (!r.get_float(out.y)) return false;
    if (!r.get_float(out.z)) return false;
    return true;
}

// Tipe attach label 3D (mirror LABEL_ATTACH_* di server).
enum class LabelAttach : uint8_t {
    None    = 0,  // statis: x,y,z = posisi dunia
    Player  = 1,  // attach ke pemain: x,y,z = offset
    Vehicle = 2,  // attach ke kendaraan: x,y,z = offset
};

struct LabelSpawnInfo {
    uint16_t labelid = 0;
    LabelAttach attach_type = LabelAttach::None;
    uint16_t attach_id = 0xFFFF;  // pid/vid; 0xFFFF bila statis
    float x = 0, y = 0, z = 0;    // statis: posisi; attached: offset
    float drawdist = 0;
    uint32_t color = 0;           // 0xRRGGBBAA ala SA-MP
    std::string text;             // UTF-8
};

// 0x28 LABEL_SPAWN: labelid(u16) + attach_type(u8) + attach_id(u16) +
//   x,y,z(float) + drawdist(float) + color(u32) + textlen(u8) + text.
inline bool parse_label_spawn(const uint8_t* data, size_t len,
                              LabelSpawnInfo& out) {
    Reader r(data, len);
    uint8_t atype = 0, tlen = 0;
    if (!r.get_u16(out.labelid)) return false;
    if (!r.get_u8(atype)) return false;
    if (!r.get_u16(out.attach_id)) return false;
    if (!r.get_float(out.x)) return false;
    if (!r.get_float(out.y)) return false;
    if (!r.get_float(out.z)) return false;
    if (!r.get_float(out.drawdist)) return false;
    if (!r.get_u32(out.color)) return false;
    if (!r.get_u8(tlen)) return false;
    if (!r.get_str(out.text, tlen)) return false;
    out.attach_type = static_cast<LabelAttach>(atype);
    return true;
}

struct LabelTextInfo {
    uint16_t labelid = 0;
    uint32_t color = 0;  // 0xRRGGBBAA
    std::string text;
};

// 0x2A LABEL_TEXT: labelid(u16) + color(u32) + textlen(u8) + text.
inline bool parse_label_text(const uint8_t* data, size_t len,
                             LabelTextInfo& out) {
    Reader r(data, len);
    uint8_t tlen = 0;
    if (!r.get_u16(out.labelid)) return false;
    if (!r.get_u32(out.color)) return false;
    if (!r.get_u8(tlen)) return false;
    if (!r.get_str(out.text, tlen)) return false;
    return true;
}

struct LabelPosInfo {
    uint16_t labelid = 0;
    float x = 0, y = 0, z = 0;
};

// 0x2B LABEL_POS: labelid(u16) + x,y,z(float). Hanya label statis.
inline bool parse_label_pos(const uint8_t* data, size_t len,
                            LabelPosInfo& out) {
    Reader r(data, len);
    if (!r.get_u16(out.labelid)) return false;
    if (!r.get_float(out.x)) return false;
    if (!r.get_float(out.y)) return false;
    if (!r.get_float(out.z)) return false;
    return true;
}

struct ObjectSpawnInfo {
    uint16_t objid = 0;
    uint32_t modelid = 0;
    float x = 0, y = 0, z = 0;
    float rx = 0, ry = 0, rz = 0;  // rotasi derajat
    float drawdist = 0;
    int32_t vw = 0;  // -1 = semua world
};

// 0x2C OBJECT_SPAWN: objid(u16) + modelid(u32) + x,y,z(float) +
//   rx,ry,rz(float) + drawdist(float) + vw(i32).
inline bool parse_object_spawn(const uint8_t* data, size_t len,
                               ObjectSpawnInfo& out) {
    Reader r(data, len);
    uint32_t vw = 0;
    if (!r.get_u16(out.objid)) return false;
    if (!r.get_u32(out.modelid)) return false;
    if (!r.get_float(out.x)) return false;
    if (!r.get_float(out.y)) return false;
    if (!r.get_float(out.z)) return false;
    if (!r.get_float(out.rx)) return false;
    if (!r.get_float(out.ry)) return false;
    if (!r.get_float(out.rz)) return false;
    if (!r.get_float(out.drawdist)) return false;
    if (!r.get_u32(vw)) return false;
    out.vw = static_cast<int32_t>(vw);
    return true;
}

struct ObjectMoveInfo {
    uint16_t objid = 0;
    float tx = 0, ty = 0, tz = 0;  // target
    float speed = 0;               // unit/detik
    float trx = 0, try_ = 0, trz = 0;  // target rotasi (-1000 = tetap)
};

// 0x2E OBJECT_MOVE: objid(u16) + tx,ty,tz(float) + speed(float) +
//   trx,try,trz(float).
inline bool parse_object_move(const uint8_t* data, size_t len,
                              ObjectMoveInfo& out) {
    Reader r(data, len);
    if (!r.get_u16(out.objid)) return false;
    if (!r.get_float(out.tx)) return false;
    if (!r.get_float(out.ty)) return false;
    if (!r.get_float(out.tz)) return false;
    if (!r.get_float(out.speed)) return false;
    if (!r.get_float(out.trx)) return false;
    if (!r.get_float(out.try_)) return false;
    if (!r.get_float(out.trz)) return false;
    return true;
}

struct ObjectUpdateInfo {
    uint16_t objid = 0;
    float x = 0, y = 0, z = 0;
    float rx = 0, ry = 0, rz = 0;
};

// 0x2F OBJECT_UPDATE: objid(u16) + x,y,z(float) + rx,ry,rz(float).
inline bool parse_object_update(const uint8_t* data, size_t len,
                                ObjectUpdateInfo& out) {
    Reader r(data, len);
    if (!r.get_u16(out.objid)) return false;
    if (!r.get_float(out.x)) return false;
    if (!r.get_float(out.y)) return false;
    if (!r.get_float(out.z)) return false;
    if (!r.get_float(out.rx)) return false;
    if (!r.get_float(out.ry)) return false;
    if (!r.get_float(out.rz)) return false;
    return true;
}

// ---- Actor statis ala SA-MP (server -> client) ----

struct ActorSpawnInfo {
    uint16_t actorid = 0;
    uint16_t modelid = 0;  // skin id SA-MP 0-311
    float x = 0, y = 0, z = 0;
    float angle = 0;
    uint16_t health = 0;
    bool invulnerable = false;
    uint32_t vw = 0;
};

// 0x32 ACTOR_SPAWN: actorid(u16) + modelid(u16) + x,y,z(float) +
//   angle(float) + health(u16) + invuln(u8) + vw(u32).
inline bool parse_actor_spawn(const uint8_t* data, size_t len,
                              ActorSpawnInfo& out) {
    Reader r(data, len);
    uint8_t inv = 0;
    if (!r.get_u16(out.actorid)) return false;
    if (!r.get_u16(out.modelid)) return false;
    if (!r.get_float(out.x)) return false;
    if (!r.get_float(out.y)) return false;
    if (!r.get_float(out.z)) return false;
    if (!r.get_float(out.angle)) return false;
    if (!r.get_u16(out.health)) return false;
    if (!r.get_u8(inv)) return false;
    if (!r.get_u32(out.vw)) return false;
    out.invulnerable = (inv != 0);
    return true;
}

struct ActorPosInfo {
    uint16_t actorid = 0;
    float x = 0, y = 0, z = 0;
    float angle = 0;
};

// 0x34 ACTOR_POS: actorid(u16) + x,y,z(float) + angle(float).
inline bool parse_actor_pos(const uint8_t* data, size_t len,
                            ActorPosInfo& out) {
    Reader r(data, len);
    if (!r.get_u16(out.actorid)) return false;
    if (!r.get_float(out.x)) return false;
    if (!r.get_float(out.y)) return false;
    if (!r.get_float(out.z)) return false;
    if (!r.get_float(out.angle)) return false;
    return true;
}

struct ActorAnimInfo {
    uint16_t actorid = 0;
    std::string anim;  // "" = animasi dibersihkan
};

// 0x35 ACTOR_ANIM: actorid(u16) + animlen(u8) + anim.
inline bool parse_actor_anim(const uint8_t* data, size_t len,
                             ActorAnimInfo& out) {
    Reader r(data, len);
    uint8_t alen = 0;
    if (!r.get_u16(out.actorid)) return false;
    if (!r.get_u8(alen)) return false;
    if (!r.get_str(out.anim, alen)) return false;
    return true;
}

struct ActorHealthInfo {
    uint16_t actorid = 0;
    uint16_t health = 0;
};

// 0x36 ACTOR_HEALTH: actorid(u16) + health(u16).
inline bool parse_actor_health(const uint8_t* data, size_t len,
                               ActorHealthInfo& out) {
    Reader r(data, len);
    if (!r.get_u16(out.actorid)) return false;
    if (!r.get_u16(out.health)) return false;
    return true;
}

struct AudioPlayInfo {
    std::string url;        // URL audio (UTF-8), tanpa null-terminator
    bool usepos = false;    // true = posisional 3D
    float x = 0, y = 0, z = 0;  // posisi sumber (bila posisional)
    float distance = 50;    // jarak dengar maksimal (bila posisional)
    bool loop = false;      // true = putar ulang terus
};

// 0x37 AUDIO_PLAY: urllen(u16) + url + usepos(u8) + x,y,z(float) +
//   distance(float) + loop(u8).
inline bool parse_audio_play(const uint8_t* data, size_t len,
                             AudioPlayInfo& out) {
    Reader r(data, len);
    uint16_t ulen = 0;
    uint8_t usepos = 0, loop = 0;
    if (!r.get_u16(ulen)) return false;
    if (!r.get_str(out.url, ulen)) return false;
    if (!r.get_u8(usepos)) return false;
    if (!r.get_float(out.x)) return false;
    if (!r.get_float(out.y)) return false;
    if (!r.get_float(out.z)) return false;
    if (!r.get_float(out.distance)) return false;
    if (!r.get_u8(loop)) return false;
    out.usepos = (usepos != 0);
    out.loop = (loop != 0);
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
