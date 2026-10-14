// ============================================================
// ROMP_Net — TCP client non-blocking untuk RO-MP.
// Hanya pakai POSIX socket + C++ standar (tanpa API Android),
// supaya bisa di-test compile standalone di Linux.
// ============================================================
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "ROMP_Protocol.h"

namespace ROMP {

class ROMPNet {
public:
    // Callback tiap paket lengkap diterima: type + payload mentah.
    using PacketCallback = std::function<void(PacketType type,
                                             const uint8_t* payload, size_t len)>;

    ROMPNet();
    ~ROMPNet();

    ROMPNet(const ROMPNet&) = delete;
    ROMPNet& operator=(const ROMPNet&) = delete;

    void SetPacketCallback(PacketCallback cb) { on_packet_ = std::move(cb); }

    // Connect ke host:port (blocking dengan timeout_ms; socket dibuat
    // non-blocking setelah connect sukses supaya Pump() tidak blokir).
    // Mengembalikan true jika TCP tersambung.
    bool Connect(const std::string& host, uint16_t port, int timeout_ms = 5000);

    // Tutup koneksi (aman dipanggil berkali-kali).
    void Close();

    bool IsConnected() const { return fd_ >= 0; }

    // Kirim satu frame lengkap. Mengembalikan false jika gagal.
    bool SendFrame(const std::vector<uint8_t>& frame);

    // Dipanggil tiap frame game: baca data masuk, rakit frame, panggil
    // callback untuk tiap paket lengkap. Mengembalikan false jika koneksi
    // putus/error (pemanggil sebaiknya Close + handle disconnect).
    bool Pump();

private:
    // Coba ambil satu frame dari rx_buf_. Mengembalikan true jika sebuah
    // paket lengkap berhasil diekstrak ke out_type/out_payload.
    bool TryExtractFrame(PacketType& out_type, std::vector<uint8_t>& out_payload);

    int fd_ = -1;
    std::vector<uint8_t> rx_buf_;
    std::vector<uint8_t> tx_buf_;  // sisa kirim yang belum terkirim (partial send)
    PacketCallback on_packet_;
    bool desync_ = false;  // true jika header frame tidak valid
};

}  // namespace ROMP
