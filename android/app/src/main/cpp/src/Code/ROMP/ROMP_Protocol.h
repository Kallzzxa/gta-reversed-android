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
    // World settings ala SA-MP (SetGravity/SetWeather/SetWorldTime).
    WORLD_SET      = 0x39,  // gravity/weather/time dunia (global)
    // Komponen kendaraan ala SA-MP (AddVehicleComponent, paintjob).
    VEH_COMP        = 0x3A,  // komponen dipasang: vid + compid + slot
    VEH_COMP_REMOVE = 0x3B,  // komponen dilepas: vid + compid
    VEH_PAINTJOB    = 0x3C,  // paintjob berubah: vid + paintjob
    // Params kendaraan ala SA-MP (SetVehicleParamsEx/ForPlayer).
    VEH_PARAMS   = 0x3D,  // params global berubah: vid + 7x int8
    VEH_PARAMS_P = 0x3E,  // params per-pemain: vid + pid + 2x int8
    // Attached object ala SA-MP (SetPlayerAttachedObject /
    // RemovePlayerAttachedObject), server -> client.
    ATTACH_OBJ   = 0x3F,  // set: pid+index+modelid+bone+9xf32 (43B payload);
                          // remove: pid+index+modelid=0 (6B payload)
    // Weapon skill ala SA-MP (SetPlayerSkillLevel), server -> client.
    SKILL_SET    = 0x43,  // level berubah: pid+skill+u16 level (4B payload)
    // Player marker ala SA-MP (ShowPlayerMarkers/SetPlayerMarkerForPlayer/
    // GetPlayerColor), server -> client.
    MARKER_SET   = 0x44,  // warna marker: u8 pid + u32 color LE (5B payload)
    MARKER_MODE  = 0x45,  // mode marker global: u8 mode (1B payload)
    // Kill feed ala SA-MP (SendDeathMessage / OnPlayerDeath).
    DEATH_REPORT = 0x46,  // client -> server: lapor kematian,
                          // u8 killerid + u8 reason (2B payload);
                          // victim = pengirim; killerid 255 = tak ada pembunuh
    DEATH_MSG    = 0x47,  // server -> semua client: kill feed,
                          // u8 killerid + u8 victimid + u8 reason (3B payload)
    // Wanted level ala SA-MP (SetPlayerWantedLevel/GetPlayerWantedLevel),
    // server -> semua client (broadcast global).
    WANTED_SET   = 0x48,  // level wanted berubah: u8 pid + u8 level
    TEAM_SET     = 0x53,  // tim pemain berubah: u8 pid + u8 team
                          // (255 = TEAM_NONE, tanpa tim ala NO_TEAM)
                          // (0=bersih .. 6=paling dicari) (2B payload)
    // Override dunia per-pemain ala SA-MP (SetPlayerTime/SetPlayerWeather/
    // TogglePlayerClock), server -> client (hanya ke pemain yang dituju).
    CLOCK_SET    = 0x54,  // toggle jam: u8 pid + u8 clock_enabled
                          // (1=jalan, 0=dibekukan) (2B payload)
    // Uang pemain otoritatif ala SA-MP (SetPlayerMoney/GivePlayerMoney/
    // ResetPlayerMoney/GetPlayerMoney), server -> client (per-pemain,
    // hanya ke pemilik — uang hanya tampil di HUD milik sendiri).
    MONEY_SET    = 0x49,  // saldo berubah: u8 pid + s32 amount LE
                          // (-999999999..999999999) (5B payload)
    // Plat nomor kendaraan ala SA-MP (SetVehicleNumberPlate), server ->
    // client (broadcast vw-scoped + resync saat join / pindah vw).
    VEH_PLATE    = 0x4A,  // plat berubah: u16 vid + u8 len + bytes(len)
                          // (len 0..32, "" = tak ada plat)
    // Spectate ala SA-MP (TogglePlayerSpectating/PlayerSpectatePlayer/
    // PlayerSpectateVehicle), server -> client (broadcast vw-scoped ke
    // vw penonton + resync saat join / pindah vw).
    SPECTATE     = 0x4B,  // u8 pid + u8 spectating + u8 target_type +
                          // u16 target LE + u8 mode (6B payload);
                          // target_type 0=none/1=pemain/2=kendaraan,
                          // mode 0=normal/1=fixed/2=side
    // Key state ala SA-MP (GetPlayerKeys / OnPlayerKeyStateChange):
    // client -> server kirim key state-nya sendiri; server -> client
    // broadcast key state pemain lain ke viewer.
    KEY_STATE    = 0x4C,  // client -> server: u16 keys LE (2B payload)
    KEY_SYNC     = 0x4D,  // server -> client: u8 pid + u16 keys LE
                          // (3B payload); broadcast vw-scoped ke viewer
                          // yang melihat pemain itu + resync saat join /
                          // pindah vw
    // Animasi pemain ala SA-MP (romp.applyAnimationForPlayer /
    // romp.clearAnimationsForPlayer), server -> client.
    ANIM_SET     = 0x4E,  // u8 pid + u8 liblen + lib + u8 namelen + name +
                          // u8 flags + u16 timems LE
                          // (liblen/namelen 1..32; flags: bit0=loop,
                          // bit1=lockx, bit2=locky, bit3=freeze,
                          // bit4=forcesync)
    ANIM_CLEAR   = 0x4F,  // u8 pid (hentikan animasi, ala ClearAnimations)
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

struct WorldSetInfo {
    float gravity = 0.008f;  // gravitasi dunia (default GTA:SA)
    uint8_t weather = 10;    // id cuaca (10 = Sunny)
    uint8_t hour = 12;       // jam dunia 0-23
    uint8_t minute = 0;      // menit dunia 0-59
};

// 0x39 WORLD_SET: gravity(f32) + weather(u8) + hour(u8) + minute(u8).
inline bool parse_world_set(const uint8_t* data, size_t len,
                            WorldSetInfo& out) {
    Reader r(data, len);
    if (!r.get_float(out.gravity)) return false;
    if (!r.get_u8(out.weather)) return false;
    if (!r.get_u8(out.hour)) return false;
    if (!r.get_u8(out.minute)) return false;
    return true;
}

struct VehicleCompInfo {
    uint16_t vid = 0;     // vehicleid
    uint16_t compid = 0;  // id komponen SA-MP (1000..1179)
    uint8_t slot = 0;     // slot 0..15 (0=spoiler, 6=exhaust, 7=wheels, ...)
};

// 0x3A VEH_COMP: vid(u16) + compid(u16) + slot(u8).
inline bool parse_vehicle_comp(const uint8_t* data, size_t len,
                               VehicleCompInfo& out) {
    Reader r(data, len);
    if (!r.get_u16(out.vid)) return false;
    if (!r.get_u16(out.compid)) return false;
    if (!r.get_u8(out.slot)) return false;
    return true;
}

struct VehicleCompRemoveInfo {
    uint16_t vid = 0;
    uint16_t compid = 0;
};

// 0x3B VEH_COMP_REMOVE: vid(u16) + compid(u16).
inline bool parse_vehicle_comp_remove(const uint8_t* data, size_t len,
                                      VehicleCompRemoveInfo& out) {
    Reader r(data, len);
    if (!r.get_u16(out.vid)) return false;
    if (!r.get_u16(out.compid)) return false;
    return true;
}

struct VehiclePaintjobInfo {
    uint16_t vid = 0;
    uint8_t paintjob = 255;  // 0..2, 255 = dilepas
};

// 0x3C VEH_PAINTJOB: vid(u16) + paintjob(u8).
inline bool parse_vehicle_paintjob(const uint8_t* data, size_t len,
                                   VehiclePaintjobInfo& out) {
    Reader r(data, len);
    if (!r.get_u16(out.vid)) return false;
    if (!r.get_u8(out.paintjob)) return false;
    return true;
}

struct VehicleParamsInfo {
    uint16_t vid = 0;
    // -1 = unset, 0 = mati, 1 = nyala (ala SA-MP SetVehicleParamsEx).
    int8_t engine = -1;
    int8_t lights = -1;
    int8_t alarm = -1;
    int8_t doors = -1;
    int8_t bonnet = -1;
    int8_t boot = -1;
    int8_t objective = -1;
};

// 0x3D VEH_PARAMS: vid(u16) + 7x int8.
inline bool parse_vehicle_params(const uint8_t* data, size_t len,
                                 VehicleParamsInfo& out) {
    Reader r(data, len);
    if (!r.get_u16(out.vid)) return false;
    uint8_t v;
    int8_t* f[7] = {&out.engine, &out.lights, &out.alarm, &out.doors,
                    &out.bonnet, &out.boot, &out.objective};
    for (int i = 0; i < 7; ++i) {
        if (!r.get_u8(v)) return false;
        *f[i] = static_cast<int8_t>(v);
    }
    return true;
}

struct VehicleParamsForPlayerInfo {
    uint16_t vid = 0;
    uint16_t pid = 0;
    int8_t objective = -1;    // -1 = unset, 0 = mati, 1 = nyala
    int8_t doorslocked = -1;  // -1 = unset, 0 = buka, 1 = kunci
};

// 0x3E VEH_PARAMS_P: vid(u16) + pid(u16) + 2x int8.
inline bool parse_vehicle_params_p(const uint8_t* data, size_t len,
                                   VehicleParamsForPlayerInfo& out) {
    Reader r(data, len);
    if (!r.get_u16(out.vid)) return false;
    if (!r.get_u16(out.pid)) return false;
    uint8_t v;
    if (!r.get_u8(v)) return false;
    out.objective = static_cast<int8_t>(v);
    if (!r.get_u8(v)) return false;
    out.doorslocked = static_cast<int8_t>(v);
    return true;
}

// ---- Attached object ala SA-MP (server -> client) ----

struct AttachObjInfo {
    uint8_t pid = 0;       // pemilik (u8, cocok dengan id pemain server)
    uint8_t index = 0;     // slot 0-9
    uint32_t modelid = 0;  // model objek SA-MP; 0 = remove
    uint8_t bone = 0;      // 1-18 ala SA-MP (1=spine, 2=head, ..., 18=jaw)
    float off[3] = {0, 0, 0};    // offset posisi dari bone
    float rot[3] = {0, 0, 0};    // rotasi euler (derajat)
    float scale[3] = {1, 1, 1};  // skala per sumbu
};

// 0x3F ATTACH_OBJ: u8 pid | u8 index | u32 modelid | u8 bone |
//   f32 offx,offy,offz | f32 rotx,roty,rotz | f32 scalex,scaley,scalez
//   (43 byte payload) -> set; atau u8 pid | u8 index | u32 modelid=0
//   (6 byte payload) -> remove. Set vs remove dibedakan dari panjang
//   payload / modelid.
inline bool parse_attach_obj(const uint8_t* data, size_t len,
                             AttachObjInfo& out) {
    Reader r(data, len);
    if (!r.get_u8(out.pid)) return false;
    if (!r.get_u8(out.index)) return false;
    if (!r.get_u32(out.modelid)) return false;
    if (len == 6) return out.modelid == 0;  // remove: modelid harus 0
    if (!r.get_u8(out.bone)) return false;
    for (int i = 0; i < 3; i++) {
        if (!r.get_float(out.off[i])) return false;
    }
    for (int i = 0; i < 3; i++) {
        if (!r.get_float(out.rot[i])) return false;
    }
    for (int i = 0; i < 3; i++) {
        if (!r.get_float(out.scale[i])) return false;
    }
    return true;
}

// ---- Weapon skill ala SA-MP (server -> client) ----

struct SkillSetInfo {
    uint8_t pid = 0;      // pemain yang skill-nya berubah (u8)
    uint8_t skill = 0;    // id skill 0-10 ala SA-MP (WEAPONSKILL_*)
    uint16_t level = 0;   // level 0-1000
};

// 0x43 SKILL_SET: u8 pid | u8 skill | u16 level (LE) (4 byte payload).
inline bool parse_skill_set(const uint8_t* data, size_t len,
                            SkillSetInfo& out) {
    Reader r(data, len);
    if (!r.get_u8(out.pid)) return false;
    if (!r.get_u8(out.skill)) return false;
    if (!r.get_u16(out.level)) return false;
    return len == 4;  // tolak payload kepanjangan/kependekan
}

// ---- Player marker ala SA-MP (server -> client) ----

struct MarkerSetInfo {
    uint8_t pid = 0;       // pemain yang warna markernya berubah (u8)
    uint32_t color = 0;    // warna marker efektif 0xRRGGBBAA (u32 LE)
};

// 0x44 MARKER_SET: u8 pid | u32 color (LE) (5 byte payload).
inline bool parse_marker_set(const uint8_t* data, size_t len,
                             MarkerSetInfo& out) {
    Reader r(data, len);
    if (!r.get_u8(out.pid)) return false;
    if (!r.get_u32(out.color)) return false;
    return len == 5;  // tolak payload kepanjangan/kependekan
}

// 0x45 MARKER_MODE: u8 mode (1 byte payload: 0=off, 1=global, 2=streamed).
inline bool parse_marker_mode(const uint8_t* data, size_t len,
                              uint8_t& out_mode) {
    if (len != 1) return false;
    out_mode = data[0];
    return out_mode <= 2;
}

// Info kill feed ala SA-MP (0x47 DEATH_MSG).
struct DeathMsgInfo {
    uint8_t killerid = 255;  // 255 = tak ada pembunuh (DEATH_KILLER_NONE)
    uint8_t victimid = 0;    // pemain yang mati
    uint8_t reason = 0;      // id senjata ala SA-MP / kode khusus 49..54
};

// 0x47 DEATH_MSG: u8 killerid | u8 victimid | u8 reason (3 byte payload).
inline bool parse_death_msg(const uint8_t* data, size_t len,
                            DeathMsgInfo& out) {
    Reader r(data, len);
    if (!r.get_u8(out.killerid)) return false;
    if (!r.get_u8(out.victimid)) return false;
    if (!r.get_u8(out.reason)) return false;
    return len == 3;  // tolak payload kepanjangan/kependekan
}

// ---- Wanted level ala SA-MP (server -> client, broadcast global) ----

// Info wanted level ala SA-MP (0x48 WANTED_SET).
struct WantedSetInfo {
    uint8_t pid = 0;    // pemain yang wanted-nya berubah (u8)
    uint8_t level = 0;  // level wanted 0=bersih .. 6=paling dicari (u8)
};

// 0x48 WANTED_SET: u8 pid | u8 level (2 byte payload).
inline bool parse_wanted_set(const uint8_t* data, size_t len,
                             WantedSetInfo& out) {
    Reader r(data, len);
    if (!r.get_u8(out.pid)) return false;
    if (!r.get_u8(out.level)) return false;
    return len == 2;  // tolak payload kepanjangan/kependekan
}

// ---- Tim pemain ala SA-MP (server -> client, broadcast global) ----

// Info tim pemain ala SA-MP (0x53 TEAM_SET).
struct TeamSetInfo {
    uint8_t pid = 0;   // pemain yang timnya berubah (u8)
    uint8_t team = 0;  // id tim 0..254; 255 = TEAM_NONE (tanpa tim) (u8)
};

// 0x53 TEAM_SET: u8 pid | u8 team (2 byte payload).
inline bool parse_team_set(const uint8_t* data, size_t len,
                           TeamSetInfo& out) {
    Reader r(data, len);
    if (!r.get_u8(out.pid)) return false;
    if (!r.get_u8(out.team)) return false;
    return len == 2;  // tolak payload kepanjangan/kependekan
}

// ---- Toggle jam pemain ala SA-MP (server -> client, per-pemain) ----

// Info toggle jam pemain ala SA-MP (0x54 CLOCK_SET).
struct ClockSetInfo {
    uint8_t pid = 0;       // pemain yang jamnya di-toggle (u8)
    bool enabled = true;   // true = jam berjalan, false = dibekukan (u8)
};

// 0x54 CLOCK_SET: u8 pid | u8 clock_enabled (2 byte payload).
inline bool parse_clock_set(const uint8_t* data, size_t len,
                            ClockSetInfo& out) {
    Reader r(data, len);
    uint8_t on = 0;
    if (!r.get_u8(out.pid)) return false;
    if (!r.get_u8(on)) return false;
    out.enabled = (on != 0);
    return len == 2;  // tolak payload kepanjangan/kependekan
}

// ---- Uang pemain otoritatif ala SA-MP (server -> client, per-pemain) ----

// Info saldo pemain ala SA-MP (0x49 MONEY_SET).
struct MoneySetInfo {
    uint8_t pid = 0;       // pemain yang saldonya berubah (u8)
    int32_t amount = 0;    // saldo baru, s32 LE (-999999999..999999999)
};

// 0x49 MONEY_SET: u8 pid | s32 amount LE (5 byte payload).
inline bool parse_money_set(const uint8_t* data, size_t len,
                            MoneySetInfo& out) {
    Reader r(data, len);
    if (!r.get_u8(out.pid)) return false;
    uint32_t u;
    if (!r.get_u32(u)) return false;
    out.amount = static_cast<int32_t>(u);
    return len == 5;  // tolak payload kepanjangan/kependekan
}

// ---- Plat nomor kendaraan ala SA-MP (server -> client) ----

// Info plat nomor kendaraan ala SA-MP (0x4A VEH_PLATE).
struct PlateInfo {
    uint16_t vid = 0;      // id kendaraan (u16 LE)
    std::string plate;     // teks plat, maks 32 char ("" = tak ada plat)
};

// 0x4A VEH_PLATE: u16 vid | u8 len | bytes(len) (payload 3..35 byte).
inline bool parse_plate(const uint8_t* data, size_t len,
                        PlateInfo& out) {
    Reader r(data, len);
    if (!r.get_u16(out.vid)) return false;
    uint8_t plen;
    if (!r.get_u8(plen)) return false;
    if (plen > 32) return false;
    if (!r.get_str(out.plate, plen)) return false;
    return len == 3 + plen;  // tolak payload kepanjangan/kependekan
}

// ---- Spectate ala SA-MP (server -> client) ----

// Info status spectate pemain ala SA-MP (0x4B SPECTATE).
struct SpectateInfo {
    uint8_t pid = 0;         // pemain yang (berhenti) spectate (u8)
    uint8_t spectating = 0;  // 0 = berhenti, 1 = spectate (u8)
    uint8_t target_type = 0; // 0=none, 1=pemain, 2=kendaraan (u8)
    uint16_t target = 0;     // pid atau vid target, u16 LE (0 bila none)
    uint8_t mode = 0;        // 0=normal, 1=fixed, 2=side (u8)
};

// 0x4B SPECTATE: u8 pid | u8 spectating | u8 target_type | u16 target LE |
// u8 mode (6 byte payload).
inline bool parse_spectate(const uint8_t* data, size_t len,
                           SpectateInfo& out) {
    Reader r(data, len);
    if (!r.get_u8(out.pid)) return false;
    if (!r.get_u8(out.spectating)) return false;
    if (!r.get_u8(out.target_type)) return false;
    if (!r.get_u16(out.target)) return false;
    if (!r.get_u8(out.mode)) return false;
    return len == 6;  // tolak payload kepanjangan/kependekan
}

// ---- Key state ala SA-MP (GetPlayerKeys / OnPlayerKeyStateChange) ----

// Info key state pemain ala SA-MP (0x4D KEY_SYNC).
struct KeySyncInfo {
    uint8_t pid = 0;    // pemain yang tombolnya berubah (u8)
    uint16_t keys = 0;  // bitmask tombol ala SA-MP (u16 LE):
                        // KEY_ACTION=1, KEY_CROUCH=2, KEY_FIRE=4,
                        // KEY_SPRINT=8, KEY_SECONDARY_ATTACK=16,
                        // KEY_JUMP=32, KEY_LOOK_RIGHT=64,
                        // KEY_HANDBRAKE=128, KEY_LOOK_LEFT=256,
                        // KEY_SUBMISSION=512, KEY_WALK=1024,
                        // KEY_ANALOG_UP=2048, KEY_ANALOG_DOWN=4096,
                        // KEY_ANALOG_RIGHT=8192, KEY_ANALOG_LEFT=16384
                        // (bit >16, mis. KEY_YES=65536, tak muat di
                        // payload u16 dan dipotong saat kirim)
};

// 0x4D KEY_SYNC: u8 pid | u16 keys LE (3 byte payload).
inline bool parse_key_sync(const uint8_t* data, size_t len,
                           KeySyncInfo& out) {
    Reader r(data, len);
    if (!r.get_u8(out.pid)) return false;
    if (!r.get_u16(out.keys)) return false;
    return len == 3;  // tolak payload kepanjangan/kependekan
}

// 0x4C KEY_STATE (C->S): u16 keys LE (2 byte payload). Hanya 16 bit
// terendah yang dikirim (wire u16); bit >16 ala SA-MP dipotong.
inline std::vector<uint8_t> build_key_state(uint32_t keys) {
    std::vector<uint8_t> p;
    put_u16(p, static_cast<uint16_t>(keys & 0xFFFFu));
    return make_frame(PacketType::KEY_STATE, p);
}

// ---- Animasi pemain ala SA-MP (ApplyAnimation / ClearAnimations) ----

// Info animasi pemain ala SA-MP (0x4E ANIM_SET).
struct AnimSetInfo {
    uint8_t pid = 0;       // pemain yang beranimasi (u8)
    std::string animlib;   // library animasi GTA (1..32 char, mis. "DANCING")
    std::string animname;  // nama animasi (1..32 char, mis. "dnce_m_a")
    uint8_t flags = 0;     // bit0=loop, bit1=lockx, bit2=locky,
                           // bit3=freeze, bit4=forcesync
    uint16_t timems = 0;   // durasi (ms) ala SA-MP
};

// 0x4E ANIM_SET: u8 pid | u8 liblen | lib | u8 namelen | name | u8 flags |
// u16 timems LE (payload 6..68 byte).
inline bool parse_anim_set(const uint8_t* data, size_t len,
                           AnimSetInfo& out) {
    Reader r(data, len);
    uint8_t llen = 0, nlen = 0;
    if (!r.get_u8(out.pid)) return false;
    if (!r.get_u8(llen)) return false;
    if (llen < 1 || llen > 32) return false;
    if (!r.get_str(out.animlib, llen)) return false;
    if (!r.get_u8(nlen)) return false;
    if (nlen < 1 || nlen > 32) return false;
    if (!r.get_str(out.animname, nlen)) return false;
    if (!r.get_u8(out.flags)) return false;
    if (!r.get_u16(out.timems)) return false;
    return r.remaining() == 0;  // tolak payload kepanjangan
}

// 0x4F ANIM_CLEAR: u8 pid (1 byte payload).
inline bool parse_anim_clear(const uint8_t* data, size_t len,
                             uint8_t& out_pid) {
    Reader r(data, len);
    if (!r.get_u8(out_pid)) return false;
    return r.remaining() == 0;  // tolak payload kepanjangan
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
