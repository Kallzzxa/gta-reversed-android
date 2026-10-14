// ============================================================
// ROMP_Players — implementasi tabel + interpolasi linear.
// ============================================================

#include "ROMP_Players.h"

#include <cmath>

namespace ROMP {

void ROMPPlayers::Add(uint16_t id, const std::string& name, float x, float y, float z) {
    auto it = players_.find(id);
    if (it != players_.end()) {
        it->second.name = name;
        return;
    }
    RemotePlayer p;
    p.id = id;
    p.name = name;
    p.x = p.tx = x;
    p.y = p.ty = y;
    p.z = p.tz = z;
    p.snap = true;
    players_.emplace(id, std::move(p));
}

bool ROMPPlayers::Remove(uint16_t id) {
    return players_.erase(id) > 0;
}

void ROMPPlayers::UpdateTarget(uint16_t id, float x, float y, float z,
                               float heading, PlayerState state, uint16_t vehicle_model) {
    auto it = players_.find(id);
    if (it == players_.end()) {
        // PLAYER_POS untuk pemain yang belum dikenal (mis. race JOIN vs POS):
        // buat entri sementara dengan nama placeholder.
        RemotePlayer p;
        p.id = id;
        p.name = "???";
        p.x = p.tx = x;
        p.y = p.ty = y;
        p.z = p.tz = z;
        p.heading = p.theading = heading;
        p.state = state;
        p.vehicle_model = vehicle_model;
        p.snap = true;
        players_.emplace(id, std::move(p));
        return;
    }
    RemotePlayer& p = it->second;
    p.tx = x;
    p.ty = y;
    p.tz = z;
    p.theading = heading;
    p.state = state;
    p.vehicle_model = vehicle_model;
    p.snap = false;
}

void ROMPPlayers::Interpolate(float dt, float alpha) {
    if (dt <= 0.0f) return;
    // Faktor lerp frame-rate independent: 1 - exp(-alpha * dt).
    float t = 1.0f - std::exp(-alpha * dt);
    for (auto& [id, p] : players_) {
        if (p.snap) {
            p.x = p.tx; p.y = p.ty; p.z = p.tz;
            p.heading = p.theading;
            p.snap = false;
            continue;
        }
        p.x += (p.tx - p.x) * t;
        p.y += (p.ty - p.y) * t;
        p.z += (p.tz - p.z) * t;
        // Heading: lerp sudut via jalur terpendek.
        float dh = p.theading - p.heading;
        while (dh > 180.0f) dh -= 360.0f;
        while (dh < -180.0f) dh += 360.0f;
        p.heading += dh * t;
    }
}

const RemotePlayer* ROMPPlayers::Get(uint16_t id) const {
    auto it = players_.find(id);
    return it != players_.end() ? &it->second : nullptr;
}

RemotePlayer* ROMPPlayers::Get(uint16_t id) {
    auto it = players_.find(id);
    return it != players_.end() ? &it->second : nullptr;
}

std::vector<uint16_t> ROMPPlayers::GetAllIds() const {
    std::vector<uint16_t> ids;
    ids.reserve(players_.size());
    for (const auto& [id, p] : players_) ids.push_back(id);
    return ids;
}

void ROMPPlayers::Clear() {
    players_.clear();
}

}  // namespace ROMP
