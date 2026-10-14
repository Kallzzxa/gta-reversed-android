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

**NUI — UI ala FiveM (HTML/JS).** Lua bisa mengirim pesan JSON ke WebView di
client, dan JavaScript bisa callback balik ke Lua. Demo bawaan: ketik `/inv`
di dalam game untuk membuka inventory ala ox_inventory.

```lua
local json = dofile("json.lua")

-- Server -> UI (paket 0x40). JSON bebas, 'action' = konvensi dengan halaman HTML.
romp.sendNUIMessage(id, json.encode({ action = "openInv", items = {...} }))
-- Minta UI ambil alih sentuhan (paket 0x42), mirip SetNuiFocus di FiveM.
romp.setNUIFocus(id, true)

-- UI -> server (paket 0x41). Dipanggil saat JS memanggil
-- window.romp.postMessage("nui_inv_use", JSON.stringify({...})).
romp.onNUICallback("nui_inv_use", function(pid, name, dataJson)
    local d = json.decode(dataJson)
    romp.sendChatToPlayer(pid, romp.KIND_INFO, "*** Kamu memakai: " .. d.name)
end)
```

Halaman UI: `android/app/src/main/assets/romp/nui/inventory.html`
(contoh; dibundle ke APK). Jembatan WebView: contoh
`android/.../java/com/regta/romp/ROMPWebView.java` (JS `onRomMessage`,
bridge `window.romp.postMessage`, fokus sentuhan; JNI TODO seperti chat).

**Event tambahan:**

| Event | Keterangan |
|---|---|
| `onGamemodeInit()` | Sekali saat gamemode dimuat |
| `onConsoleCommand(cmd, args)` | Return `true` = di-handle (tambah command console) |
| `onPlayerRequestConnect(name, ip)` | Return `true, "alasan"` = tolak koneksi (whitelist/ban di Lua) |

**API tambahan:**

| Fungsi | Keterangan |
|---|---|
| `romp.getPlayerIP(id)` | IP pemain / nil |
| `romp.setHostname(text)` / `romp.setMOTD(text)` / `romp.setPassword(text)` | Ubah config saat runtime |
| `romp.getServerInfo()` | Table `{hostname, motd, port, maxplayers, players, hasPassword, query, announce}` |
| `romp.banIP(ip)` / `romp.unbanIP(ip)` / `romp.isIPBanned(ip)` | Ban IP (in-memory, dicek saat handshake) |
| `romp.announceNow()` | Paksa announce ke master sekarang |

## Server Browser & Master Server

Biar pemain bisa lihat server yang nyala tanpa hafal IP:

1. **Query UDP** — server menjawab info + daftar pemain di port yang sama
   (opsi `query 1` di server.cfg).
2. **LAN discovery (tanpa VPS)** — server broadcast beacon tiap 5 detik
   (`lan_discovery 1`, default nyala); client di WiFi yang sama otomatis
   menemukan server. Murni server -> client.
3. **Master server** — untuk daftar internet-wide: jalankan
   `start-master.sh` (atau `.bat`) di VPS, default port 7778.
   Game server daftarkan diri via `announce 1` + `master_host`/`master_port`.
4. **Client** — modul `ROMP_Browser` (C++): `ListenLan()` untuk LAN,
   `FetchMasterList()` untuk daftar dari master, `QueryServer()` untuk
   info live + ping per server.
   Contoh UI: `ROMPServerBrowser.java` + `assets/romp/browser.html`
   (tab LAN / Internet / Favorit tersimpan lokal, ketuk untuk connect;
   JNI TODO seperti modul lain).

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
