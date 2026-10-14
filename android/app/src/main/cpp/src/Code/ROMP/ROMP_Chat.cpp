// ============================================================
// ROMP_Chat — implementasi ring buffer.
// ============================================================

#include "ROMP_Chat.h"

namespace ROMP {

void ROMPChat::AddLine(ChatKind kind, const std::string& text, uint64_t now_ms) {
    ChatLine line;
    line.kind = kind;
    line.text = text;
    line.time_ms = now_ms;

    if (count_ < kChatHistorySize) {
        lines_[(head_ + count_) % kChatHistorySize] = std::move(line);
        ++count_;
    } else {
        // Penuh: timpa baris tertua, geser head.
        lines_[head_] = std::move(line);
        head_ = (head_ + 1) % kChatHistorySize;
    }
}

void ROMPChat::AddSystemLine(const std::string& text, uint64_t now_ms) {
    AddLine(ChatKind::Server, text, now_ms);
}

size_t ROMPChat::GetVisibleLines(ChatLine* out, size_t max_out) const {
    if (!out || max_out == 0) return 0;
    // Ambil min(count, visible, max_out) baris TERAKHIR.
    size_t want = count_;
    if (want > kChatVisibleLines) want = kChatVisibleLines;
    if (want > max_out) want = max_out;
    size_t start = (head_ + count_ - want) % kChatHistorySize;
    for (size_t i = 0; i < want; ++i) {
        out[i] = lines_[(start + i) % kChatHistorySize];
    }
    return want;
}

void ROMPChat::Clear() {
    head_ = 0;
    count_ = 0;
}

void ROMPChat::SetInput(const std::string& text) {
    input_ = text.substr(0, kChatInputMax);
}

std::string ROMPChat::TakeInput() {
    std::string t = std::move(input_);
    input_.clear();
    return t;
}

}  // namespace ROMP
