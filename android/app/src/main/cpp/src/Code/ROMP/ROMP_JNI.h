// ============================================================
// ROMP_JNI — jembatan JNI antara Java (Android) dan ROMP::Client.
//
// File ini + ROMP_JNI.cpp mengimplementasikan SEMUA method native
// yang dideklarasikan di Java:
//
//   com.regta.romp.ROMPChatInput      -> sendChatNative
//   com.regta.romp.ROMPWebView        -> nuiCallbackNative
//   com.regta.romp.ROMPServerBrowser  -> nativeFetchMasterList,
//                                      nativeQueryServer,
//                                      nativeListenLan
//   com.regta.romp.ROMPClient         -> nativeInit, nativeConnect,
//                                      nativeDisconnect, nativeUpdate,
//                                      nativeIsConnected, nativeSendChat,
//                                      nativeGetChatLines, nativeGetPlayers,
//                                      nativeGetServerInfo, nativeSetLocalPos,
//                                      nativeGetPingMs, nativeGetLocalId,
//                                      nativeGetLocalNick, nativeGetLastReject
//
// Alur data:
//   Java -> C++ : JNI export di bawah (dipanggil dari UI/background thread)
//   C++ -> Java : AndroidPlatform::OnNuiMessage -> ROMPWebView.postToUI(json)
//                 AndroidPlatform::OnNuiFocus   -> ROMPWebView.setFocus(f)
//
// Threading: semua pemanggilan Java dari thread native men-attach thread
// ke JVM bila perlu (lalu detach setelah selesai) supaya aman dipanggil
// dari game loop maupun dari network thread.
// ============================================================
#pragma once

#include <jni.h>

#include <atomic>
#include <string>

#include "ROMP.h"
#include "ROMP_PedStreamer.h"

namespace ROMP {

// ------------------------------------------------------------
// AndroidPlatform — IROMPPlatform untuk game Android.
// ------------------------------------------------------------
class AndroidPlatform : public IROMPPlatform {
public:
    AndroidPlatform();
    ~AndroidPlatform() override;

    AndroidPlatform(const AndroidPlatform&) = delete;
    AndroidPlatform& operator=(const AndroidPlatform&) = delete;

    // Simpan JVM + global ref ke ROMPWebView (untuk OnNuiMessage/Focus).
    // nui_view boleh null (NUI mati -> pesan NUI di-log saja).
    // Mengembalikan false bila attach gagal.
    bool Attach(JavaVM* vm, jobject nui_view);

    // Lepas global ref. Panggil saat activity dihancurkan.
    void Detach();

    // Set posisi pemain lokal dari game (CPed). Dipanggil tiap frame
    // sebelum Client::Update() supaya POS yang dikirim akurat.
    void SetLocalPos(float x, float y, float z, float heading);

    // Pasang implementasi dunia game nyata (ReversedGameWorld) saat class
    // game sudah tersedia. Default: NullGameWorld (no-op tercatat).
    void SetGameWorld(IGameWorld* game_world) { peds_.Attach(game_world); }

    // Akses streamer ped (untuk test / debug).
    ROMPPedStreamer& PedStreamer() { return peds_; }

    // ---- IROMPPlatform ----
    void GetLocalPlayerPos(float& x, float& y, float& z, float& heading,
                           PlayerState& state, uint16_t& vehicle_model) override;
    void TeleportLocalPlayer(float x, float y, float z) override;
    void SpawnRemotePlayer(uint16_t id, const std::string& name) override;
    void RemoveRemotePlayer(uint16_t id) override;
    void MoveRemotePlayer(uint16_t id, float x, float y, float z,
                          float heading) override;
    void Log(const std::string& msg) override;
    void OnNuiMessage(const std::string& json) override;
    void OnNuiFocus(bool focused) override;

private:
    // Ambil JNIEnv thread ini; attach bila belum. *out_attached = true
    // bila pemanggil harus DetachCurrentThread() setelah selesai.
    JNIEnv* Env(bool* out_attached);

    JavaVM* vm_ = nullptr;
    jobject nui_view_ = nullptr;  // global ref ke com.regta.romp.ROMPWebView
    jmethodID mid_post_to_ui_ = nullptr;  // void postToUI(String)
    jmethodID mid_set_focus_ = nullptr;   // void setFocus(boolean)

    std::atomic<float> lx_{0.0f};
    std::atomic<float> ly_{0.0f};
    std::atomic<float> lz_{0.0f};
    std::atomic<float> lheading_{0.0f};

    // Streamer ped pemain remote (lifecycle spawn/move/remove + baca
    // posisi lokal dari game). Game world nyata dipasang via
    // SetGameWorld() saat reversal menyediakan class game.
    ROMPPedStreamer peds_;
};

// Singleton platform Android (dipakai ROMPClient.nativeInit).
AndroidPlatform& GetAndroidPlatform();

// ------------------------------------------------------------
// Helper JNI (dipakai implementasi di ROMP_JNI.cpp).
// ------------------------------------------------------------
std::string JniToStdString(JNIEnv* env, jstring s);
jstring StdStringToJni(JNIEnv* env, const std::string& s);

// Escape string untuk disisip ke JSON ("...\").
std::string JsonEscape(const std::string& s);

}  // namespace ROMP
