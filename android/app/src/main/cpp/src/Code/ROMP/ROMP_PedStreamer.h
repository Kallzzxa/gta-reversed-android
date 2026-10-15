// ============================================================
// ROMP_PedStreamer — lifecycle ped pemain remote di sisi client.
//
// Masalah: modul ROMP (portable) tahu KAPAN pemain remote join/leave
// dan DI MANA posisinya tiap frame (interpolasi), tapi TIDAK tahu cara
// membuat CPed di game — seluruh tree game (Peds/, World/, ...) di repo
// ini masih stub kosong, menunggu reversal.
//
// Solusi: semua sentuhan ke game dipusatkan di interface kecil
// IGameWorld. Logika portable (peta id->ped, cegah spawn ganda,
// terapkan transform tiap frame, baca posisi pemain lokal, bersih saat
// disconnect) tinggal di ROMPPedStreamer dan 100% bisa di-test tanpa
// game lewat NullGameWorld.
//
// Saat reversal sudah menyediakan class game asli, cukup tulis SATU
// file: class ReversedGameWorld : IGameWorld yang memetakan:
//
//   IsReady()          -> dunia game sudah init (mis. CGame::Initialise)
//   SpawnPed(model)    -> CStreaming::RequestModel(model, 0) +
//                        CStreaming::LoadAllRequestedModels(false),
//                        lalu CPed* p = new CPlayerPed(...); p di-set
//                        model CJ; CWorld::Add(p); AI task dimatikan
//                        supaya ped diam & full dikontrol network.
//                        model 0 = skin CJ (default SA-MP).
//   DestroyPed(h)      -> CWorld::Remove(ped); delete ped;
//   SetPedTransform    -> CPlaceable::SetPosition(x, y, z) +
//                        SetHeading(heading) pada ped.
//   GetLocalTransform  -> FindPlayerPed()->GetPosition() + heading.
//
// Pola ini sama dengan yang dipakai client SA-MP (remote player =
// actor/CPED dengan AI off, posisi di-set langsung tiap frame).
// ============================================================
#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace ROMP {

// Model ped default untuk pemain remote (0 = skin CJ ala SA-MP).
inline constexpr int kDefaultPedModel = 0;

// Transform sederhana: posisi + heading (derajat).
struct PedTransform {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float heading = 0.0f;
};

// ------------------------------------------------------------
// IGameWorld — satu-satunya titik sentuh ke game asli.
// Implementasi nyata (ReversedGameWorld) ditulis saat class game
// sudah tersedia; sampai saat itu pakai NullGameWorld.
// ------------------------------------------------------------
class IGameWorld {
public:
    using PedHandle = uint64_t;  // 0 = invalid
    static constexpr PedHandle kInvalidPed = 0;

    virtual ~IGameWorld() = default;

    // true bila dunia game siap menerima spawn (engine sudah jalan).
    virtual bool IsReady() = 0;

    // Buat ped di dunia game. Kembalikan handle != 0, atau 0 bila gagal.
    virtual PedHandle SpawnPed(uint16_t net_id, const std::string& name,
                               int model_id) = 0;

    // Hancurkan ped yang dibuat SpawnPed.
    virtual void DestroyPed(PedHandle handle) = 0;

    // Terapkan posisi + heading ke ped (dipanggil tiap frame).
    virtual void SetPedTransform(PedHandle handle, const PedTransform& t) = 0;

    // Baca transform pemain lokal dari game. false = game belum siap
    // (pemanggil harus pakai sumber posisi lain / cache).
    virtual bool GetLocalTransform(PedTransform& out) = 0;
};

// ------------------------------------------------------------
// NullGameWorld — implementasi no-op yang MENCATAT panggilan.
// Dipakai sebagai default (game belum ada) dan sebagai test double.
// ------------------------------------------------------------
class NullGameWorld : public IGameWorld {
public:
    bool IsReady() override { return ready_; }
    PedHandle SpawnPed(uint16_t net_id, const std::string& name,
                       int model_id) override;
    void DestroyPed(PedHandle handle) override;
    void SetPedTransform(PedHandle handle, const PedTransform& t) override;
    bool GetLocalTransform(PedTransform& out) override;

    // ---- kontrol untuk test ----
    void SetReady(bool r) { ready_ = r; }
    void SetLocalTransform(const PedTransform& t) {
        local_ = t;
        local_valid_ = true;
    }

    // ---- observasi untuk test ----
    int spawn_count() const { return spawn_count_; }
    int destroy_count() const { return destroy_count_; }
    int set_count() const { return set_count_; }
    // Transform terakhir yang di-set per handle (0 jika tak ada).
    PedTransform LastSet(PedHandle handle) const;

private:
    bool ready_ = true;
    PedTransform local_;
    bool local_valid_ = false;

    int spawn_count_ = 0;
    int destroy_count_ = 0;
    int set_count_ = 0;
    PedHandle next_handle_ = 1;
    std::unordered_map<PedHandle, PedTransform> last_set_;
};

// ------------------------------------------------------------
// ROMPPedStreamer — pemilik lifecycle ped remote.
//
// Dipakai IROMPPlatform: SpawnRemotePlayer -> SpawnPlayer,
// RemoveRemotePlayer -> RemovePlayer, MoveRemotePlayer -> MovePlayer,
// GetLocalPlayerPos -> ReadLocalPlayer (fallback bila game tak siap).
// Thread-safe (dipanggil dari pump thread & JNI/game thread).
// ------------------------------------------------------------
class ROMPPedStreamer {
public:
    ROMPPedStreamer();
    ~ROMPPedStreamer() = default;

    ROMPPedStreamer(const ROMPPedStreamer&) = delete;
    ROMPPedStreamer& operator=(const ROMPPedStreamer&) = delete;

    // Pasang implementasi game. Tidak mengambil ownership (pointer mentah).
    // Default: NullGameWorld internal.
    void Attach(IGameWorld* game_world);

    // Spawn ped untuk pemain remote id. Idempotent: spawn kedua untuk id
    // yang sama mengembalikan true tanpa membuat ped baru.
    // false bila game belum siap / SpawnPed gagal.
    bool SpawnPlayer(uint16_t id, const std::string& name,
                     int model_id = kDefaultPedModel);

    // Hapus ped pemain id. true bila ped memang ada dan dihancurkan.
    bool RemovePlayer(uint16_t id);

    // Terapkan posisi+heading hasil interpolasi ke ped. false bila id
    // tidak dikenal (ped belum di-spawn / sudah dihapus).
    bool MovePlayer(uint16_t id, float x, float y, float z, float heading);

    // Hancurkan semua ped (dipanggil saat disconnect).
    void Clear();

    // Baca transform pemain lokal dari game. false bila game belum siap.
    bool ReadLocalPlayer(float& x, float& y, float& z, float& heading);

    size_t Count() const;
    bool Has(uint16_t id) const;

private:
    struct Entry {
        IGameWorld::PedHandle handle = IGameWorld::kInvalidPed;
        std::string name;
    };

    IGameWorld* game_ = nullptr;
    NullGameWorld null_game_;  // default bila Attach() tak dipanggil
    mutable std::mutex mutex_;
    std::unordered_map<uint16_t, Entry> peds_;
};

}  // namespace ROMP
