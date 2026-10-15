// ============================================================
// ROMP_PedStreamer — implementasi.
// Lihat ROMP_PedStreamer.h untuk desain & peta integrasi game.
// ============================================================

#include "ROMP_PedStreamer.h"

namespace ROMP {

// ---------------- NullGameWorld ----------------

IGameWorld::PedHandle NullGameWorld::SpawnPed(uint16_t net_id,
                                              const std::string& name,
                                              int model_id) {
    (void)net_id;
    (void)name;
    (void)model_id;
    if (!ready_) return kInvalidPed;
    ++spawn_count_;
    return next_handle_++;
}

void NullGameWorld::DestroyPed(PedHandle handle) {
    if (handle == kInvalidPed) return;
    ++destroy_count_;
    last_set_.erase(handle);
}

void NullGameWorld::SetPedTransform(PedHandle handle, const PedTransform& t) {
    if (handle == kInvalidPed) return;
    ++set_count_;
    last_set_[handle] = t;
}

bool NullGameWorld::GetLocalTransform(PedTransform& out) {
    if (!ready_ || !local_valid_) return false;
    out = local_;
    return true;
}

PedTransform NullGameWorld::LastSet(PedHandle handle) const {
    auto it = last_set_.find(handle);
    if (it == last_set_.end()) return PedTransform{};
    return it->second;
}

// ---------------- ROMPPedStreamer ----------------

ROMPPedStreamer::ROMPPedStreamer() : game_(&null_game_) {}

void ROMPPedStreamer::Attach(IGameWorld* game_world) {
    std::lock_guard<std::mutex> lock(mutex_);
    game_ = game_world ? game_world : &null_game_;
}

bool ROMPPedStreamer::SpawnPlayer(uint16_t id, const std::string& name,
                                  int model_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = peds_.find(id);
    if (it != peds_.end()) return true;  // sudah ada -> idempotent

    if (!game_->IsReady()) return false;
    IGameWorld::PedHandle h = game_->SpawnPed(id, name, model_id);
    if (h == IGameWorld::kInvalidPed) return false;

    peds_[id] = Entry{h, name};
    return true;
}

bool ROMPPedStreamer::RemovePlayer(uint16_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = peds_.find(id);
    if (it == peds_.end()) return false;
    game_->DestroyPed(it->second.handle);
    peds_.erase(it);
    return true;
}

bool ROMPPedStreamer::MovePlayer(uint16_t id, float x, float y, float z,
                                 float heading) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = peds_.find(id);
    if (it == peds_.end()) return false;
    PedTransform t;
    t.x = x;
    t.y = y;
    t.z = z;
    t.heading = heading;
    game_->SetPedTransform(it->second.handle, t);
    return true;
}

void ROMPPedStreamer::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& kv : peds_) game_->DestroyPed(kv.second.handle);
    peds_.clear();
}

bool ROMPPedStreamer::ReadLocalPlayer(float& x, float& y, float& z,
                                      float& heading) {
    std::lock_guard<std::mutex> lock(mutex_);
    PedTransform t;
    if (!game_->GetLocalTransform(t)) return false;
    x = t.x;
    y = t.y;
    z = t.z;
    heading = t.heading;
    return true;
}

size_t ROMPPedStreamer::Count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return peds_.size();
}

bool ROMPPedStreamer::Has(uint16_t id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return peds_.find(id) != peds_.end();
}

}  // namespace ROMP
