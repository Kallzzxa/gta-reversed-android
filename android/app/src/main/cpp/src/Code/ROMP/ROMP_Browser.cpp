// ============================================================
// ROMP_Browser — implementasi server browser.
// ============================================================
#include "ROMP_Browser.h"

#include <chrono>
#include <cstring>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
   typedef SOCKET sock_t;
#  define CLOSESOCK(s) closesocket(s)
#  define INVALID_SOCK INVALID_SOCKET
#else
#  include <unistd.h>
#  include <errno.h>
#  include <fcntl.h>
#  include <netdb.h>
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <sys/select.h>
   typedef int sock_t;
#  define CLOSESOCK(s) ::close(s)
#  define INVALID_SOCK (-1)
#endif

namespace ROMP {
namespace {

// Magic + tipe query (mirror PROTOCOL.md).
const char kMagic[4] = { 'R', 'O', 'M', 'P' };
const uint8_t kQInfo = 0x01, kQInfoReply = 0x02;
const uint8_t kQPlayers = 0x03, kQPlayersReply = 0x04;
const uint8_t kMListReq = 0x51, kMList = 0x52;

uint16_t rd_u16(const uint8_t* b) { return (uint16_t)(b[0] | (b[1] << 8)); }

bool resolve(const std::string& host, uint16_t port, sockaddr_in& out) {
    memset(&out, 0, sizeof(out));
    out.sin_family = AF_INET;
    out.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &out.sin_addr) == 1) return true;
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* ai = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &ai) != 0 || !ai) return false;
    out.sin_addr = ((sockaddr_in*)ai->ai_addr)->sin_addr;
    freeaddrinfo(ai);
    return true;
}

bool wait_readable(sock_t s, int timeout_ms) {
    fd_set rf; FD_ZERO(&rf); FD_SET(s, &rf);
    timeval tv{ timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
#ifdef _WIN32
    return select(0, &rf, nullptr, nullptr, &tv) > 0;
#else
    return select((int)(s + 1), &rf, nullptr, nullptr, &tv) > 0;
#endif
}

bool parse_info_reply(const uint8_t* p, size_t n, BrowserServer& out) {
    // p[0] = Q_INFO_REPLY sudah dicek pemanggil
    size_t o = 1;
    auto get_str = [&](std::string& s) -> bool {
        if (o + 1 > n) return false;
        uint8_t l = p[o++];
        if (o + l > n) return false;
        s.assign((const char*)(p + o), l); o += l;
        return true;
    };
    if (!get_str(out.hostname)) return false;
    if (!get_str(out.motd)) return false;
    if (!get_str(out.version)) return false;
    if (o + 5 > n) return false;
    out.players = rd_u16(p + o); o += 2;
    out.maxplayers = rd_u16(p + o); o += 2;
    out.has_password = p[o++] != 0;
    return true;
}

bool parse_players_reply(const uint8_t* p, size_t n, BrowserServer& out) {
    size_t o = 1;  // lewati Q_PLAYERS_REPLY
    if (o + 1 > n) return false;
    uint8_t cnt = p[o++];
    out.player_list.clear();
    for (uint8_t i = 0; i < cnt; i++) {
        if (o + 3 > n) return false;
        BrowserPlayer pl;
        pl.id = rd_u16(p + o); o += 2;
        uint8_t l = p[o++];
        if (o + l > n) return false;
        pl.name.assign((const char*)(p + o), l); o += l;
        out.player_list.push_back(std::move(pl));
    }
    return true;
}

sock_t tcp_connect_timeout(const sockaddr_in& dst, int timeout_ms) {
    sock_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCK) return INVALID_SOCK;
#ifdef _WIN32
    u_long m = 1; ioctlsocket(s, FIONBIO, &m);
#else
    int f = fcntl(s, F_GETFL, 0); fcntl(s, F_SETFL, f | O_NONBLOCK);
#endif
    int rc = connect(s, (const sockaddr*)&dst, sizeof(dst));
    if (rc != 0) {
#ifdef _WIN32
        if (WSAGetLastError() != WSAEWOULDBLOCK) { CLOSESOCK(s); return INVALID_SOCK; }
#else
        if (errno != EINPROGRESS) { CLOSESOCK(s); return INVALID_SOCK; }
#endif
        fd_set wf; FD_ZERO(&wf); FD_SET(s, &wf);
        timeval tv{ timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
#ifdef _WIN32
        if (select(0, nullptr, &wf, nullptr, &tv) <= 0) { CLOSESOCK(s); return INVALID_SOCK; }
#else
        if (select((int)(s + 1), nullptr, &wf, nullptr, &tv) <= 0) { CLOSESOCK(s); return INVALID_SOCK; }
#endif
        int err = 0; socklen_t el = sizeof(err);
        getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&err, &el);
        if (err != 0) { CLOSESOCK(s); return INVALID_SOCK; }
    }
    return s;
}

bool tcp_send_all(sock_t s, const uint8_t* b, size_t n) {
    while (n > 0) {
        ssize_t r = send(s, (const char*)b, n, 0);
        if (r <= 0) return false;
        b += r; n -= (size_t)r;
    }
    return true;
}

bool tcp_recv_all(sock_t s, uint8_t* b, size_t n, int timeout_ms) {
    while (n > 0) {
        if (!wait_readable(s, timeout_ms)) return false;
        ssize_t r = recv(s, (char*)b, n, 0);
        if (r <= 0) return false;
        b += r; n -= (size_t)r;
    }
    return true;
}

}  // namespace

bool ROMPBrowser::QueryServer(const std::string& host, uint16_t port,
                              BrowserServer& out, int timeout_ms) {
    out = BrowserServer{};
    out.ip = host; out.port = port;
    sockaddr_in dst{};
    if (!resolve(host, port, dst)) return false;

    uint8_t reply[1024];
    // Query info — ukur juga RTT.
    sock_t s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == INVALID_SOCK) return false;
    uint8_t req[5]; memcpy(req, kMagic, 4);
    auto t0 = std::chrono::steady_clock::now();
    req[4] = kQInfo;
    bool ok = false;
    if (sendto(s, (const char*)req, 5, 0,
               (const sockaddr*)&dst, sizeof(dst)) == 5 &&
        wait_readable(s, timeout_ms)) {
        sockaddr_in from{}; socklen_t fl = sizeof(from);
        ssize_t n = recvfrom(s, (char*)reply, sizeof(reply), 0,
                             (sockaddr*)&from, &fl);
        if (n >= 6 && !memcmp(reply, kMagic, 4) && reply[4] == kQInfoReply &&
            parse_info_reply(reply + 5, (size_t)n - 5, out)) {
            out.ping_ms = (int)std::chrono::duration_cast<
                std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - t0).count();
            ok = true;
        }
    }
    // Query pemain (best-effort, tak menggagalkan info).
    if (ok) {
        req[4] = kQPlayers;
        if (sendto(s, (const char*)req, 5, 0,
                   (const sockaddr*)&dst, sizeof(dst)) == 5 &&
            wait_readable(s, timeout_ms / 2 + 200)) {
            sockaddr_in from{}; socklen_t fl = sizeof(from);
            ssize_t n = recvfrom(s, (char*)reply, sizeof(reply), 0,
                                 (sockaddr*)&from, &fl);
            if (n >= 6 && !memcmp(reply, kMagic, 4) && reply[4] == kQPlayersReply)
                parse_players_reply(reply + 5, (size_t)n - 5, out);
        }
    }
    CLOSESOCK(s);
    return ok;
}

bool ROMPBrowser::FetchMasterList(const std::string& master_host,
                                  uint16_t master_port,
                                  std::vector<BrowserServer>& out,
                                  int timeout_ms) {
    out.clear();
    sockaddr_in dst{};
    if (!resolve(master_host, master_port, dst)) return false;
    sock_t s = tcp_connect_timeout(dst, timeout_ms);
    if (s == INVALID_SOCK) return false;
    bool ok = false;
    // Frame LIST_REQUEST: len=1, type=0x51.
    uint8_t req[3] = { 1, 0, kMListReq };
    if (tcp_send_all(s, req, 3)) {
        uint8_t hdr[2];
        if (tcp_recv_all(s, hdr, 2, timeout_ms)) {
            uint16_t ln = rd_u16(hdr);
            if (ln >= 1 && ln <= 16384) {
                std::vector<uint8_t> p(ln);
                if (tcp_recv_all(s, p.data(), ln, timeout_ms) && p[0] == kMList) {
                    size_t o = 1;
                    if (o + 2 <= p.size()) {
                        uint16_t cnt = rd_u16(p.data() + o); o += 2;
                        ok = true;
                        for (uint16_t i = 0; i < cnt && ok; i++) {
                            BrowserServer e;
                            auto get_str = [&](std::string& str) -> bool {
                                if (o + 1 > p.size()) return false;
                                uint8_t l = p[o++];
                                if (o + l > p.size()) return false;
                                str.assign((const char*)(p.data() + o), l);
                                o += l; return true;
                            };
                            if (!get_str(e.ip)) { ok = false; break; }
                            if (o + 6 > p.size()) { ok = false; break; }
                            e.port = rd_u16(p.data() + o); o += 2;
                            e.players = rd_u16(p.data() + o); o += 2;
                            e.maxplayers = rd_u16(p.data() + o); o += 2;
                            if (!get_str(e.hostname)) { ok = false; break; }
                            if (!get_str(e.motd)) { ok = false; break; }
                            out.push_back(std::move(e));
                        }
                    }
                }
            }
        }
    }
    CLOSESOCK(s);
    return ok;
}

bool ROMPBrowser::ListenLan(std::vector<BrowserServer>& out, int timeout_ms) {
    out.clear();
    sock_t s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == INVALID_SOCK) return false;
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&one, sizeof(one));
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = INADDR_ANY;
    a.sin_port = htons(7779);  // LAN_BEACON_PORT
    bool bound = bind(s, (sockaddr*)&a, sizeof(a)) == 0;
    if (!bound) { CLOSESOCK(s); return false; }

    auto end = std::chrono::steady_clock::now() +
               std::chrono::milliseconds(timeout_ms);
    uint8_t buf[512];
    while (std::chrono::steady_clock::now() < end) {
        int remain = (int)std::chrono::duration_cast<std::chrono::milliseconds>(
                         end - std::chrono::steady_clock::now()).count();
        if (remain <= 0) break;
        if (!wait_readable(s, remain > 500 ? 500 : remain)) continue;
        sockaddr_in from{}; socklen_t fl = sizeof(from);
        ssize_t n = recvfrom(s, (char*)buf, sizeof(buf), 0,
                             (sockaddr*)&from, &fl);
        // Beacon: "ROMP" 0x11 gameport(u16) hostnamelen(u8) hostname
        //         players(u16) maxplayers(u16) has_password(u8)
        //         motdlen(u8) motd
        if (n < 4 + 1 + 2 + 1 || memcmp(buf, kMagic, 4) || buf[4] != 0x11)
            continue;
        size_t o = 5;
        BrowserServer e;
        char ipstr[48];
        inet_ntop(AF_INET, &from.sin_addr, ipstr, sizeof(ipstr));
        e.ip = ipstr;
        e.port = (uint16_t)(buf[o] | (buf[o + 1] << 8)); o += 2;
        uint8_t hl = buf[o++];
        if (o + hl + 5 > (size_t)n) continue;
        e.hostname.assign((char*)(buf + o), hl); o += hl;
        e.players = (uint16_t)(buf[o] | (buf[o + 1] << 8)); o += 2;
        e.maxplayers = (uint16_t)(buf[o] | (buf[o + 1] << 8)); o += 2;
        e.has_password = buf[o++] != 0;
        uint8_t ml = buf[o++];
        if (o + ml > (size_t)n) continue;
        e.motd.assign((char*)(buf + o), ml); o += ml;
        e.version = "romp1";
        e.ping_ms = 0;  // LAN, anggap 0
        // Dedupe by ip+port: timpa dengan beacon terbaru.
        bool found = false;
        for (auto& x : out) {
            if (x.ip == e.ip && x.port == e.port) { x = e; found = true; break; }
        }
        if (!found) out.push_back(std::move(e));
    }
    CLOSESOCK(s);
    return true;
}

}  // namespace ROMP
