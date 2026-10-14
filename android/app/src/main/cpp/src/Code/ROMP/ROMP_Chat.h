// ============================================================
// ROMP_Chat — ring buffer chat ala SA-MP.
// Menampilkan N baris terakhir, menyimpan histori lebih banyak.
// + input buffer untuk teks yang sedang diketik user.
// ============================================================
#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "ROMP_Protocol.h"

namespace ROMP {

// Jumlah baris yang tampil di layar (seperti chatbox SA-MP).
static constexpr size_t kChatVisibleLines = 12;
// Total histori yang disimpan.
static constexpr size_t kChatHistorySize = 100;
// Panjang maksimal teks input user (sama dengan batas server).
static constexpr size_t kChatInputMax = 128;

struct ChatLine {
    ChatKind kind = ChatKind::Normal;  // warna dikaitkan ke kind
    std::string text;                  // teks polos (sudah diformat server)
    uint64_t time_ms = 0;              // timestamp baris masuk (ms)
};

class ROMPChat {
public:
    ROMPChat() = default;

    // Tambah satu baris chat. Baris tertua dibuang saat penuh.
    void AddLine(ChatKind kind, const std::string& text, uint64_t now_ms);

    // Shortcut untuk pesan sistem lokal (mis. "Terhubung ke server").
    void AddSystemLine(const std::string& text, uint64_t now_ms);

    // Ambil N baris terakhir untuk dirender. count=0 -> semua yang tampil.
    // Mengembalikan jumlah baris yang ditulis ke out (maks visible).
    size_t GetVisibleLines(ChatLine* out, size_t max_out) const;

    size_t LineCount() const { return count_; }

    void Clear();

    // ---- Input buffer (teks yang sedang diketik user) ----
    void SetInput(const std::string& text);
    const std::string& GetInput() const { return input_; }
    void ClearInput() { input_.clear(); }
    // Ambil + kosongkan (dipakai saat user menekan Enter/kirim).
    std::string TakeInput();

private:
    std::array<ChatLine, kChatHistorySize> lines_{};
    size_t head_ = 0;   // indeks baris tertua
    size_t count_ = 0;  // jumlah baris valid
    std::string input_;
};

}  // namespace ROMP
