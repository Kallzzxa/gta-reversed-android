// ============================================================
// ROMP_Players — tabel pemain remote + interpolasi posisi.
// Server mengirim PLAYER_POS ~10 Hz; di antaranya posisi di-lerp
// supaya gerakan pemain lain terlihat mulus.
// ============================================================
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "ROMP_Protocol.h"

namespace ROMP {

struct RemotePlayer {
    uint16_t id = 0;
    std::string name;

    // Posisi saat ini (hasil interpolasi).
    float x = 0, y = 0, z = 0;
    float heading = 0;
    PlayerState state = PlayerState::OnFoot;
    uint16_t vehicle_model = 0;

    // Target interpolasi (dari PLAYER_POS terakhir).
    float tx = 0, ty = 0, tz = 0;
    float theading = 0;

    // true jika target baru belum dikonsumsi (spawn = snap langsung).
    bool snap = true;
};

class ROMPPlayers {
public:
    ROMPPlayers() = default;

    // Tambah pemain (dari PLAYER_JOIN / PLAYER_LIST). Jika id sudah ada,
    // hanya nama yang diperbarui. Posisi di-snap ke target.
    void Add(uint16_t id, const std::string& name,
             float x = 0, float y = 0, float z = 0);

    // Hapus pemain (dari PLAYER_LEAVE). Mengembalikan true jika ada.
    bool Remove(uint16_t id);

    // Update target interpolasi (dari PLAYER_POS).
    void UpdateTarget(uint16_t id, float x, float y, float z,
                      float heading, PlayerState state, uint16_t vehicle_model);

    // Maju interpolasi semua pemain. alpha = fraksi per detik menuju target
    // (nilai 8-12 terasa mulus untuk update 10 Hz).
    // Dipanggil tiap frame dengan dt detik.
    void Interpolate(float dt, float alpha = 10.0f);

    const RemotePlayer* Get(uint16_t id) const;
    RemotePlayer* Get(uint16_t id);

    // Daftar semua id pemain remote (untuk iterasi render).
    std::vector<uint16_t> GetAllIds() const;

    size_t Count() const { return players_.size(); }
    void Clear();

private:
    std::unordered_map<uint16_t, RemotePlayer> players_;
};

}  // namespace ROMP
