// ============================================================
// ROMP_JNI — implementasi jembatan JNI Java <-> ROMP::Client.
//
// Semua method native di package com.regta.romp diimplementasikan
// di sini. Format JSON yang dikembalikan mengikuti dokumentasi di
// masing-masing file Java (ROMPServerBrowser, ROMPClient).
//
// Catatan thread:
//  - nativeConnect / nativeFetchMasterList / nativeQueryServer /
//    nativeListenLan bersifat SINKRON dan bisa blokir beberapa detik:
//    panggil SELALU dari background thread (ROMPClient/ROMPServerBrowser
//    sudah melakukannya).
//  - nativeUpdate dipanggil tiap frame dari game loop (atau dari
//    auto-pump ROMPClient).
// ============================================================

#include "ROMP_JNI.h"

#include <android/log.h>
#include <cstdio>
#include <sstream>
#include <vector>

#include "ROMP_Browser.h"
#include "ROMP_Protocol.h"

#define LOG_TAG "RO-MP"

namespace ROMP {

namespace {

const char* kChatKindName[] = {"normal", "server", "me", "pm", "joinleave"};
// Warna ala SA-MP per jenis baris chat.
const char* kChatKindColor[] = {
    "#FFFFFF",  // normal : putih
    "#F0B429",  // server : emas
    "#C586C0",  // me     : ungu
    "#FFFF00",  // pm      : kuning
    "#9E9E9E",  // joinleave : abu-abu
};

std::string BrowserServerToJson(const BrowserServer& s, bool master_list_style) {
    std::ostringstream o;
    o << "{\"ip\":\"" << JsonEscape(s.ip) << "\""
      << ",\"port\":" << s.port
      << ",\"hostname\":\"" << JsonEscape(s.hostname) << "\""
      << ",\"motd\":\"" << JsonEscape(s.motd) << "\""
      << ",\"players\":" << s.players
      << ",\"maxplayers\":" << s.maxplayers;
    if (!master_list_style) {
        o << ",\"version\":\"" << JsonEscape(s.version) << "\""
          << ",\"has_password\":" << (s.has_password ? "true" : "false")
          << ",\"ping_ms\":" << s.ping_ms
          << ",\"player_list\":[";
        for (size_t i = 0; i < s.player_list.size(); ++i) {
            if (i) o << ",";
            o << "{\"id\":" << s.player_list[i].id
              << ",\"name\":\"" << JsonEscape(s.player_list[i].name) << "\"}";
        }
        o << "]";
    }
    o << "}";
    return o.str();
}

}  // namespace

// ------------------------------------------------------------
// Helper
// ------------------------------------------------------------
std::string JniToStdString(JNIEnv* env, jstring s) {
    if (!s) return "";
    const char* c = env->GetStringUTFChars(s, nullptr);
    std::string out = c ? c : "";
    if (c) env->ReleaseStringUTFChars(s, c);
    return out;
}

jstring StdStringToJni(JNIEnv* env, const std::string& s) {
    return env->NewStringUTF(s.c_str());
}

std::string JsonEscape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    o += buf;
                } else {
                    o += c;
                }
        }
    }
    return o;
}

// ------------------------------------------------------------
// AndroidPlatform
// ------------------------------------------------------------
AndroidPlatform::AndroidPlatform() = default;

AndroidPlatform::~AndroidPlatform() {
    Detach();
}

bool AndroidPlatform::Attach(JavaVM* vm, jobject nui_view) {
    if (!vm) return false;
    vm_ = vm;

    bool attached = false;
    JNIEnv* env = Env(&attached);
    if (!env) return false;

    if (nui_view) {
        nui_view_ = env->NewGlobalRef(nui_view);
        jclass cls = env->GetObjectClass(nui_view_);
        if (cls) {
            mid_post_to_ui_ = env->GetMethodID(cls, "postToUI", "(Ljava/lang/String;)V");
            mid_set_focus_ = env->GetMethodID(cls, "setFocus", "(Z)V");
            env->DeleteLocalRef(cls);
        }
        if (!mid_post_to_ui_ || !mid_set_focus_) {
            __android_log_print(ANDROID_LOG_WARN, LOG_TAG,
                                "ROMPWebView.postToUI/setFocus tidak ditemukan");
        }
    }

    if (attached) vm_->DetachCurrentThread();
    return true;
}

void AndroidPlatform::Detach() {
    if (vm_ && nui_view_) {
        bool attached = false;
        JNIEnv* env = Env(&attached);
        if (env) env->DeleteGlobalRef(nui_view_);
        if (attached) vm_->DetachCurrentThread();
    }
    nui_view_ = nullptr;
    vm_ = nullptr;
    mid_post_to_ui_ = nullptr;
    mid_set_focus_ = nullptr;
}

void AndroidPlatform::SetLocalPos(float x, float y, float z, float heading) {
    lx_.store(x);
    ly_.store(y);
    lz_.store(z);
    lheading_.store(heading);
}

// Teleport pemain lokal (paket 0x27 POS_SET dari romp.setPlayerPos).
// Perbarui cache posisi supaya POS berikutnya langsung kirim lokasi baru;
// TODO(GAME): pindahkan juga CPed lokal (CPlaceable::SetPosition).
void AndroidPlatform::TeleportLocalPlayer(float x, float y, float z) {
    lx_.store(x);
    ly_.store(y);
    lz_.store(z);
}

void AndroidPlatform::GetLocalPlayerPos(float& x, float& y, float& z,
                                        float& heading, PlayerState& state,
                                        uint16_t& vehicle_model) {
    // TODO(GAME): baca langsung dari CPed pemain lokal (FindPlayerPed()).
    // Sementara posisi disuplai game via ROMPClient.setLocalPos() tiap frame.
    x = lx_.load();
    y = ly_.load();
    z = lz_.load();
    heading = lheading_.load();
    state = PlayerState::OnFoot;
    vehicle_model = 0;
}

void AndroidPlatform::SpawnRemotePlayer(uint16_t id, const std::string& name) {
    // TODO(GAME): buat CPed / actor via CWorld::Add + beri model CJ.
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                        "SpawnRemotePlayer id=%u name=%s", id, name.c_str());
}

void AndroidPlatform::RemoveRemotePlayer(uint16_t id) {
    // TODO(GAME): hapus CPed yang dibuat di SpawnRemotePlayer.
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "RemoveRemotePlayer id=%u", id);
}

void AndroidPlatform::MoveRemotePlayer(uint16_t id, float x, float y, float z,
                                      float heading) {
    // TODO(GAME): set posisi CPed (CPlaceable::SetPosition / teleport).
    // Posisi interpolasi tetap bisa dibaca Java via nativeGetPlayers().
    (void)id; (void)x; (void)y; (void)z; (void)heading;
}

void AndroidPlatform::Log(const std::string& msg) {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", msg.c_str());
}

JNIEnv* AndroidPlatform::Env(bool* out_attached) {
    *out_attached = false;
    if (!vm_) return nullptr;
    JNIEnv* env = nullptr;
    if (vm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_EDETACHED) {
        if (vm_->AttachCurrentThread(&env, nullptr) != JNI_OK) return nullptr;
        *out_attached = true;
    }
    return env;
}

void AndroidPlatform::OnNuiMessage(const std::string& json) {
    if (!nui_view_ || !mid_post_to_ui_) {
        __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "NUI message (no view): %s",
                            json.c_str());
        return;
    }
    bool attached = false;
    JNIEnv* env = Env(&attached);
    if (!env) return;
    jstring j = StdStringToJni(env, json);
    env->CallVoidMethod(nui_view_, mid_post_to_ui_, j);
    if (j) env->DeleteLocalRef(j);
    if (attached) vm_->DetachCurrentThread();
}

void AndroidPlatform::OnNuiFocus(bool focused) {
    if (!nui_view_ || !mid_set_focus_) return;
    bool attached = false;
    JNIEnv* env = Env(&attached);
    if (!env) return;
    env->CallVoidMethod(nui_view_, mid_set_focus_, focused ? JNI_TRUE : JNI_FALSE);
    if (attached) vm_->DetachCurrentThread();
}

AndroidPlatform& GetAndroidPlatform() {
    static AndroidPlatform inst;
    return inst;
}

// ------------------------------------------------------------
// JNI: com.regta.romp.ROMPChatInput
// ------------------------------------------------------------
extern "C" JNIEXPORT void JNICALL
Java_com_regta_romp_ROMPChatInput_sendChatNative(JNIEnv* env, jobject /*thiz*/,
                                                jstring text) {
    std::string msg = JniToStdString(env, text);
    if (!Client::Instance().SendChat(msg)) {
        __android_log_print(ANDROID_LOG_WARN, LOG_TAG,
                            "sendChat gagal (tidak tersambung?)");
    }
}

// ------------------------------------------------------------
// JNI: com.regta.romp.ROMPWebView
// ------------------------------------------------------------
extern "C" JNIEXPORT void JNICALL
Java_com_regta_romp_ROMPWebView_nuiCallbackNative(JNIEnv* env, jclass /*clazz*/,
                                                 jstring name, jstring dataJson) {
    std::string n = JniToStdString(env, name);
    std::string d = JniToStdString(env, dataJson);
    // -> paket 0x41 NUI_CALLBACK ke server -> Lua romp.onNUICallback.
    if (!Client::Instance().SendNuiCallback(n, d)) {
        __android_log_print(ANDROID_LOG_WARN, LOG_TAG,
                            "nuiCallback gagal (tidak tersambung?)");
    }
}

// ------------------------------------------------------------
// JNI: com.regta.romp.ROMPServerBrowser
// ------------------------------------------------------------
extern "C" JNIEXPORT jstring JNICALL
Java_com_regta_romp_ROMPServerBrowser_nativeFetchMasterList(JNIEnv* env,
                                                           jclass /*clazz*/,
                                                           jstring host,
                                                           jint port) {
    std::vector<BrowserServer> list;
    if (!ROMPBrowser::FetchMasterList(JniToStdString(env, host),
                                      static_cast<uint16_t>(port), list)) {
        return StdStringToJni(env, "[]");
    }
    std::ostringstream o;
    o << "[";
    for (size_t i = 0; i < list.size(); ++i) {
        if (i) o << ",";
        o << BrowserServerToJson(list[i], /*master_list_style=*/true);
    }
    o << "]";
    return StdStringToJni(env, o.str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_regta_romp_ROMPServerBrowser_nativeQueryServer(JNIEnv* env,
                                                       jclass /*clazz*/,
                                                       jstring host,
                                                       jint port) {
    BrowserServer info;
    std::string h = JniToStdString(env, host);
    uint16_t p = static_cast<uint16_t>(port);
    info.ip = h;
    info.port = p;
    if (!ROMPBrowser::QueryServer(h, p, info)) {
        // Server tak menjawab: kembalikan entri minimal supaya halaman
        // bisa menampilkan "offline".
        std::ostringstream o;
        o << "{\"ip\":\"" << JsonEscape(h) << "\",\"port\":" << p
          << ",\"hostname\":\"\",\"motd\":\"\",\"version\":\"\",\"players\":0"
          << ",\"maxplayers\":0,\"has_password\":false,\"ping_ms\":-1"
          << ",\"player_list\":[],\"offline\":true}";
        return StdStringToJni(env, o.str());
    }
    return StdStringToJni(env, BrowserServerToJson(info, /*master_list_style=*/false));
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_regta_romp_ROMPServerBrowser_nativeListenLan(JNIEnv* env,
                                                     jclass /*clazz*/,
                                                     jint timeoutMs) {
    std::vector<BrowserServer> list;
    if (!ROMPBrowser::ListenLan(list, static_cast<int>(timeoutMs))) {
        return StdStringToJni(env, "[]");
    }
    std::ostringstream o;
    o << "[";
    for (size_t i = 0; i < list.size(); ++i) {
        if (i) o << ",";
        o << BrowserServerToJson(list[i], /*master_list_style=*/false);
    }
    o << "]";
    return StdStringToJni(env, o.str());
}

// ------------------------------------------------------------
// JNI: com.regta.romp.ROMPClient
// ------------------------------------------------------------
extern "C" JNIEXPORT jboolean JNICALL
Java_com_regta_romp_ROMPClient_nativeInit(JNIEnv* env, jclass /*clazz*/,
                                         jobject nuiView) {
    JavaVM* vm = nullptr;
    if (env->GetJavaVM(&vm) != JNI_OK) return JNI_FALSE;
    if (!GetAndroidPlatform().Attach(vm, nuiView)) return JNI_FALSE;
    Client::Instance().Init(&GetAndroidPlatform());
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_regta_romp_ROMPClient_nativeConnect(JNIEnv* env, jclass /*clazz*/,
                                            jstring host, jstring nick,
                                            jint port, jstring password) {
    // SINKRON, bisa blokir ~5 dtk saat handshake: panggil dari
    // background thread (ROMPClient.connect sudah melakukannya).
    bool ok = Client::Instance().Connect(JniToStdString(env, host),
                                         static_cast<uint16_t>(port),
                                         JniToStdString(env, nick),
                                         JniToStdString(env, password));
    return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_regta_romp_ROMPClient_nativeDisconnect(JNIEnv* /*env*/, jclass /*clazz*/) {
    Client::Instance().Disconnect();
}

extern "C" JNIEXPORT void JNICALL
Java_com_regta_romp_ROMPClient_nativeUpdate(JNIEnv* /*env*/, jclass /*clazz*/) {
    // Pompa network + kirim POS (100 ms) + interpolasi + PING (15 dtk).
    Client::Instance().Update();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_regta_romp_ROMPClient_nativeIsConnected(JNIEnv* /*env*/, jclass /*clazz*/) {
    return Client::Instance().IsConnected() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_regta_romp_ROMPClient_nativeSendChat(JNIEnv* env, jclass /*clazz*/,
                                             jstring text) {
    return Client::Instance().SendChat(JniToStdString(env, text)) ? JNI_TRUE
                                                                  : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_regta_romp_ROMPClient_nativeGetChatLines(JNIEnv* env, jclass /*clazz*/) {
    // [{"kind":"server","color":"#F0B429","text":"...","time":12345}, ...]
    // Urutan: tertua -> terbaru (siap di-render top-down).
    ChatLine lines[kChatVisibleLines];
    size_t n = Client::Instance().Chat().GetVisibleLines(lines, kChatVisibleLines);
    std::ostringstream o;
    o << "[";
    for (size_t i = 0; i < n; ++i) {
        if (i) o << ",";
        uint8_t k = static_cast<uint8_t>(lines[i].kind);
        if (k >= 5) k = 0;
        o << "{\"kind\":\"" << kChatKindName[k] << "\""
          << ",\"color\":\"" << kChatKindColor[k] << "\""
          << ",\"text\":\"" << JsonEscape(lines[i].text) << "\""
          << ",\"time\":" << lines[i].time_ms << "}";
    }
    o << "]";
    return StdStringToJni(env, o.str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_regta_romp_ROMPClient_nativeGetPlayers(JNIEnv* env, jclass /*clazz*/) {
    // [{"id":0,"name":"..","x":1.0,"y":2.0,"z":3.0,"heading":90.0,
    //   "is_local":true,"ping_ms":12}, ...]
    // Pemain lokal selalu entri pertama.
    Client& c = Client::Instance();
    std::ostringstream o;
    o << "[";
    float x, y, z, heading;
    PlayerState st;
    uint16_t vmodel;
    GetAndroidPlatform().GetLocalPlayerPos(x, y, z, heading, st, vmodel);
    o << "{\"id\":" << c.LocalPlayerId()
      << ",\"name\":\"" << JsonEscape(c.LocalNick()) << "\""
      << ",\"x\":" << x << ",\"y\":" << y << ",\"z\":" << z
      << ",\"heading\":" << heading
      << ",\"is_local\":true"
      << ",\"ping_ms\":" << c.GetPingMs() << "}";
    for (uint16_t id : c.Players().GetAllIds()) {
        const RemotePlayer* p = c.Players().Get(id);
        if (!p) continue;
        o << ",{\"id\":" << p->id
          << ",\"name\":\"" << JsonEscape(p->name) << "\""
          << ",\"x\":" << p->x << ",\"y\":" << p->y << ",\"z\":" << p->z
          << ",\"heading\":" << p->heading
          << ",\"is_local\":false"
          << ",\"ping_ms\":-1}";
    }
    o << "]";
    return StdStringToJni(env, o.str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_regta_romp_ROMPClient_nativeGetServerInfo(JNIEnv* env, jclass /*clazz*/) {
    Client& c = Client::Instance();
    const WelcomeInfo& w = c.ServerInfo();
    std::ostringstream o;
    o << "{\"hostname\":\"" << JsonEscape(w.server_name) << "\""
      << ",\"motd\":\"" << JsonEscape(w.motd) << "\""
      << ",\"maxplayers\":" << static_cast<int>(w.maxplayers)
      << ",\"players\":" << (c.Players().Count() + (c.IsConnected() ? 1 : 0))
      << ",\"local_id\":" << c.LocalPlayerId()
      << ",\"connected\":" << (c.IsConnected() ? "true" : "false") << "}";
    return StdStringToJni(env, o.str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_regta_romp_ROMPClient_nativeSetLocalPos(JNIEnv* /*env*/, jclass /*clazz*/,
                                                jfloat x, jfloat y, jfloat z,
                                                jfloat heading) {
    GetAndroidPlatform().SetLocalPos(x, y, z, heading);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_regta_romp_ROMPClient_nativeGetPingMs(JNIEnv* /*env*/, jclass /*clazz*/) {
    return static_cast<jint>(Client::Instance().GetPingMs());
}

extern "C" JNIEXPORT jint JNICALL
Java_com_regta_romp_ROMPClient_nativeGetLocalId(JNIEnv* /*env*/, jclass /*clazz*/) {
    return static_cast<jint>(Client::Instance().LocalPlayerId());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_regta_romp_ROMPClient_nativeGetLocalNick(JNIEnv* env, jclass /*clazz*/) {
    return StdStringToJni(env, Client::Instance().LocalNick());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_regta_romp_ROMPClient_nativeGetLastReject(JNIEnv* env, jclass /*clazz*/) {
    return StdStringToJni(env, Client::Instance().LastRejectReason());
}

}  // namespace ROMP
