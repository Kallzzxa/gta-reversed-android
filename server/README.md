# RO-MP Dedicated Server

Server multiplayer untuk **RO-MP** — versi SA-MP ala project gabut yang jalan di
atas `gta-reversed-android`. Satu binari kecil tanpa dependency, tinggal jalanin.
**Gamemode ditulis pakai Lua** (ala FiveM) — lihat [`gamemode.lua`](gamemode.lua).

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
| `gamemode` | gamemode.lua | File gamemode Lua (relatif ke folder server) |

## Scripting Gamemode (Lua, ala FiveM)

Logika gamemode ditulis di `gamemode.lua`, bukan di-hardcode di server.
Contoh bawaan sudah mencakup pengumuman join/leave custom, command
`/waktu` `/info` `/kick`, filter chat, dan timer MOTD tiap 5 menit.

**Event** (return `true` = di-handle, lewati default C):

```lua
function onPlayerJoin(id, name) ... end
function onPlayerLeave(id, name, reason) ... end  -- reason: 0=quit, 1=kick, 2=timeout
function onPlayerChat(id, name, text) ... end     -- return true = pesan diblokir
function onPlayerCommand(id, name, cmd, args) ... end
function onTick() ... end                         -- tiap 1 detik
```

**API `romp.*`:**

| Fungsi | Keterangan |
|---|---|
| `romp.log(text)` | Tulis ke log server |
| `romp.playerCount()` | Jumlah pemain online |
| `romp.getPlayerName(id)` | Nama pemain / nil |
| `romp.getPlayers()` | Array `{ {id=.., name=..}, ... }` |
| `romp.sendChatToAll(kind, text)` | Chat ke semua pemain |
| `romp.sendChatToPlayer(id, kind, text)` | Chat ke satu pemain |
| `romp.kickPlayer(id, reason)` | Kick pemain |
| `romp.getTick()` | ms sejak server nyala |
| `romp.getConfig(key)` | `hostname`/`motd`/`port`/`maxplayers` |
| `romp.KIND_CHAT/INFO/ME/PM/SYS` | Konstanta jenis chat |

Command bawaan server (`/me` `/pm` `/players` `/help` `/q`) tetap jalan sebagai
fallback kalau gamemode tidak meng-handle-nya. Error di script Lua hanya
dicatat di log — server tidak crash.

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
