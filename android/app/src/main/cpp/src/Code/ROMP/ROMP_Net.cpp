// ============================================================
// ROMP_Net — implementasi TCP client non-blocking (POSIX).
// ============================================================

#include "ROMP_Net.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

namespace ROMP {

ROMPNet::ROMPNet() = default;

ROMPNet::~ROMPNet() {
    Close();
}

bool ROMPNet::Connect(const std::string& host, uint16_t port, int timeout_ms) {
    Close();

    // Resolve hostname (bisa IP atau nama host).
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* list = nullptr;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &list) != 0) {
        return false;
    }

    int fd = -1;
    for (addrinfo* ai = list; ai != nullptr; ai = ai->ai_next) {
        fd = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;

        // Non-blocking supaya connect pakai timeout via select().
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        int rc = ::connect(fd, ai->ai_addr, ai->ai_addrlen);
        if (rc == 0) break;  // langsung tersambung (jarang)

        if (errno != EINPROGRESS) {
            ::close(fd);
            fd = -1;
            continue;
        }

        // Tunggu sampai writable atau timeout.
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(fd, &wfds);
        timeval tv{};
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        rc = ::select(fd + 1, nullptr, &wfds, nullptr, &tv);
        if (rc > 0) {
            int err = 0;
            socklen_t errlen = sizeof(err);
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errlen);
            if (err == 0) break;  // connect sukses
        }
        ::close(fd);
        fd = -1;
    }
    freeaddrinfo(list);

    if (fd < 0) return false;

    fd_ = fd;
    desync_ = false;
    rx_buf_.clear();
    tx_buf_.clear();
    return true;
}

void ROMPNet::Close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    rx_buf_.clear();
    tx_buf_.clear();
}

bool ROMPNet::SendFrame(const std::vector<uint8_t>& frame) {
    if (fd_ < 0 || frame.empty()) return false;

    // Kirim dulu sisa buffer yang belum terkirim sebelumnya.
    if (!tx_buf_.empty()) {
        ssize_t n = ::send(fd_, tx_buf_.data(), tx_buf_.size(), MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return true;  // coba lagi nanti
            return false;
        }
        tx_buf_.erase(tx_buf_.begin(), tx_buf_.begin() + n);
        if (!tx_buf_.empty()) {
            // Buffer masih penuh: tumpuk frame baru di belakang.
            tx_buf_.insert(tx_buf_.end(), frame.begin(), frame.end());
            return true;
        }
    }

    ssize_t n = ::send(fd_, frame.data(), frame.size(), MSG_NOSIGNAL);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            tx_buf_.insert(tx_buf_.end(), frame.begin(), frame.end());
            return true;
        }
        return false;
    }
    if (static_cast<size_t>(n) < frame.size()) {
        // Partial send: simpan sisanya.
        tx_buf_.insert(tx_buf_.end(), frame.begin() + n, frame.end());
    }
    return true;
}

bool ROMPNet::Pump() {
    if (fd_ < 0) return false;
    desync_ = false;

    // Cek data masuk tanpa blokir.
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd_, &rfds);
    timeval tv{};
    int rc = ::select(fd_ + 1, &rfds, nullptr, nullptr, &tv);
    if (rc < 0) return false;   // error select
    if (rc == 0) return true;   // tidak ada data, bukan error

    uint8_t tmp[8192];
    ssize_t n = ::recv(fd_, tmp, sizeof(tmp), 0);
    if (n == 0) return false;   // server menutup koneksi (EOF)
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return true;
        return false;
    }
    rx_buf_.insert(rx_buf_.end(), tmp, tmp + n);

    // Ekstrak semua frame lengkap yang tersedia.
    PacketType type;
    std::vector<uint8_t> payload;
    while (!desync_ && TryExtractFrame(type, payload)) {
        if (on_packet_) on_packet_(type, payload.data(), payload.size());
    }
    return !desync_;
}

bool ROMPNet::TryExtractFrame(PacketType& out_type, std::vector<uint8_t>& out_payload) {
    // Butuh minimal 3 byte: len(u16) + type(u8).
    if (rx_buf_.size() < 3) return false;

    uint16_t len = static_cast<uint16_t>(rx_buf_[0])
                 | (static_cast<uint16_t>(rx_buf_[1]) << 8);
    if (len == 0 || len > kMaxFrameLen) {
        // Header rusak / desync: tandai koneksi error supaya Pump()
        // mengembalikan false dan koneksi ditutup bersih.
        desync_ = true;
        rx_buf_.clear();
        return false;
    }
    if (rx_buf_.size() < static_cast<size_t>(2 + len)) return false;  // belum lengkap

    out_type = static_cast<PacketType>(rx_buf_[2]);
    out_payload.assign(rx_buf_.begin() + 3, rx_buf_.begin() + 2 + len);
    rx_buf_.erase(rx_buf_.begin(), rx_buf_.begin() + 2 + len);
    return true;
}

}  // namespace ROMP
