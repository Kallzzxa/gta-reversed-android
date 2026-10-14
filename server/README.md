# RO-MP Dedicated Server

Server multiplayer untuk **RO-MP** — versi SA-MP ala project gabut yang jalan di
atas `gta-reversed-android`. Satu binari kecil tanpa dependency, tinggal jalanin.

## Cara menjalankan

**Linux:**
```bash
chmod +x start-server.sh
./start-server.sh
```

**Windows:**
Klik dua kali `start-server.bat` (atau jalankan `bin\windows\romp-server.exe`).

Server membaca `server.cfg` di folder yang sama. Default port **7777**.

## Konfigurasi (`server.cfg`)

| Key | Default | Keterangan |
|---|---|---|
| `hostname` | RO-MP Gabut Server | Nama server |
| `motd` | Selamat datang di RO-MP! | Pesan selamat datang |
| `port` | 7777 | Port TCP |
| `maxplayers` | 50 | Maksimal pemain (1–120) |
| `password` | *(kosong)* | Password server, kosongkan = bebas |

## Command console (ketik di jendela server)

| Command | Fungsi |
|---|---|
| `help` | Daftar command |
| `say <pesan>` | Pengumuman ke semua pemain |
| `players` | Lihat pemain online |
| `kick <id> [alasan]` | Kick pemain |
| `stop` | Matikan server |

## Command chat (di dalam game)

| Command | Fungsi |
|---|---|
| `/me <aksi>` | Roleplay action (teks ungu) |
| `/pm <id> <pesan>` | Pesan pribadi |
| `/players` | Daftar pemain online |
| `/help` | Bantuan |
| `/q` | Keluar dari server |

## Fitur

- Handshake + validasi nickname (3–24 karakter, anti-duplikat)
- Broadcast chat & posisi pemain (relay 10 Hz)
- Player list, join/leave announcement, ping/pong + timeout 60 detik
- Anti-spam chat (maks 3 pesan / 2 detik)
- Password server opsional
- Log ke `romp-server.log`

## Build dari source

Butuh `gcc` (Linux) dan/atau `mingw-w64` (Windows):

```bash
cd src
make linux    # -> ../bin/linux/romp-server
make windows  # -> ../bin/windows/romp-server.exe
make all      # keduanya
```

## Protokol

Dokumentasi protokol lengkap: [PROTOCOL.md](PROTOCOL.md). Implementasi client
ada di `../android/app/src/main/cpp/src/Code/ROMP/`.
