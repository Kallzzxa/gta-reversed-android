#!/usr/bin/env python3
"""RCON client untuk RO-MP: kirim command console ke server dari terminal.

Pakai: python3 rcon.py <host> <port> <rcon_password> <command...>
Contoh: python3 rcon.py 127.0.0.1 7777 rahasia123 "say halo semua"
        python3 rcon.py 127.0.0.1 7777 rahasia123 kick 3 spam
"""
import socket
import struct
import sys


def rcon(host, port, password, cmd, timeout=5.0):
    pw = password.encode()
    cb = cmd.encode()
    if len(pw) > 63 or len(cb) > 1023 or not cb:
        raise ValueError("password maks 63 char, command 1-1023 char")
    pkt = b"ROMP" + bytes([0x05, len(pw)]) + pw + struct.pack("<H", len(cb)) + cb
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(timeout)
    try:
        s.sendto(pkt, (host, port))
        d, _ = s.recvfrom(2048)
    finally:
        s.close()
    if d[:4] != b"ROMP" or d[4] != 0x06:
        raise RuntimeError("balasan bukan RCON_REPLY")
    status = d[5]
    ln = struct.unpack("<H", d[6:8])[0]
    return status, d[8:8 + ln].decode(errors="replace")


if __name__ == "__main__":
    if len(sys.argv) < 5:
        print(__doc__.strip().splitlines()[3])
        sys.exit(2)
    host, port, pw = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    cmd = " ".join(sys.argv[4:])
    try:
        st, out = rcon(host, port, pw, cmd)
    except Exception as e:
        print(f"RCON gagal: {e}")
        sys.exit(1)
    if st != 0:
        print("RCON ditolak:", out)
        sys.exit(1)
    print(out, end="")
