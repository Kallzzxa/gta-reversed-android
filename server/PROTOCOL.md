# RO-MP Protocol v1

Protokol jaringan RO-MP (Roleplay Multiplayer) — mirip konsep SA-MP tapi
dibuat dari nol dan jauh lebih sederhana. Transport: **TCP**.

Semua integer multi-byte memakai **little-endian**. Float memakai IEEE-754
little-endian. String memakai UTF-8 tanpa null-terminator (panjang eksplisit).

## Frame

Setiap paket diawali header 2 byte:

```
u16 len   // panjang (type + payload), TIDAK termasuk 2 byte ini. Maks 4096.
u8  type  // tipe paket
u8  payload[len-1]
```

## Client -> Server

### 0x01 HELLO
Handshake pertama yang wajib dikirim client setelah connect.

```
u8  ver       // versi protokol, selalu 1
u8  namelen   // 1..24
char name[namelen]   // nickname: A-Z a-z 0-9 _ - [ ]
u8  pwlen     // 0 jika server tanpa password
char pw[pwlen]
```

Server menjawab `WELCOME` (lalu client resmi join) atau `REJECT` (lalu
koneksi ditutup).

### 0x10 POS
Posisi pemain lokal. Dikirim ~10x per detik.

```
float x, y, z   // posisi dunia
float heading   // arah hadap (derajat)
u8  state       // 0 = jalan kaki, 1 = supir, 2 = penumpang
u16 vehicle_model // model kendaraan, 0 jika jalan kaki
```

### 0x20 CHAT
Pesan chat. Maks 128 byte. Chat yang diawali `/` dianggap command.

```
u8  len
char msg[len]
```

Command bawaan server:

| Command | Fungsi |
|---|---|
| `/me <aksi>` | Roleplay action, tampil ungu: `* Andi duduk di bangku` |
| `/pm <id> <pesan>` | Private message ke pemain |
| `/players` | Daftar pemain online (dikirim sebagai pesan server) |
| `/help` | Daftar command |
| `/q` | Quit |

### 0x30 PING

```
u32 client_tick
```

Dijawab `PONG`. Client sebaiknya ping tiap 15 detik. Timeout 60 detik.

## Server -> Client

### 0x02 WELCOME

```
u16 playerid
u8  srvnamelen
char srvname[srvnamelen]
u8  motdlen
char motd[motdlen]
u8  maxplayers
```

### 0x03 REJECT

```
u8  reasonlen
char reason[reasonlen]
```

### 0x11 PLAYER_POS
Broadcast posisi pemain lain (tidak di-echo ke pengirim).

```
u16 playerid
float x, y, z
float heading
u8  state
u16 vehicle_model
```

### 0x21 CHAT_MSG

```
u16 playerid  // 0xFFFF = pesan dari server
u8  kind      // 0 = chat biasa, 1 = server/info (emas),
              // 2 = /me (ungu), 3 = PM (kuning), 4 = join/leave (abu)
u8  msglen
char msg[msglen]  // sudah diformat server, mis. "Andi (3): halo"
```

### 0x22 PLAYER_JOIN

```
u16 playerid
u8  namelen
char name[namelen]
```

### 0x23 PLAYER_LEAVE

```
u16 playerid
u8  reason  // 0 = quit, 1 = kick, 2 = timeout
```

### 0x24 PLAYER_LIST
Daftar pemain yang sedang online, dikirim tepat setelah WELCOME.

```
u8 count
// per pemain:
u16 playerid
u8  namelen
char name[namelen]
```

### 0x31 PONG

```
u32 client_tick  // echo dari PING
```

## NUI — UI ala FiveM (HTML/JS)

Pola seperti FiveM: Lua server mengirim pesan JSON ke UI (WebView di client),
dan JavaScript di UI memanggil callback balik ke Lua. Contoh: inventory ala
ox_inventory.

### 0x40 NUI_MESSAGE (Server -> Client)

```
u16 jsonlen
char json[jsonlen]  // UTF-8 JSON, maks 4000 byte. Contoh:
                    // {"action":"openInv","items":[{"slot":1,"name":"Roti","count":2}]}
```

Client meneruskan JSON apa adanya ke WebView (`onRomMessage(json)`).
`action` adalah konvensi antara gamemode Lua dan halaman HTML-nya.

### 0x41 NUI_CALLBACK (Client -> Server)

```
u8  namelen
char name[namelen]   // nama callback, mis. "nui_inv_use"
u16 datalen
char data[datalen]   // UTF-8 JSON, maks 4000 byte. Contoh: {"slot":1}
```

Diteruskan ke handler yang didaftarkan Lua via `romp.onNUICallback(name, fn)`.

### 0x42 NUI_FOCUS (Server -> Client)

```
u8 focus  // 1 = UI ambil alih sentuhan (WebView fokus), 0 = lepas ke game
```

Mirip `SetNuiFocus` di FiveM. Lua: `romp.setNUIFocus(id, true/false)`.

## Server Browser — UDP Query

Server menjawab query UDP di **port yang sama** dengan TCP (bisa dimatikan
via `query 0` di server.cfg). Format: magic `"ROMP"` (4 byte) + type(u8).
Jawaban: magic + reply_type(u8) + payload. Query dengan magic salah diabaikan.

### 0x01 QUERY_INFO (Client -> Server)

```
"ROMP" 0x01
```

### 0x02 QUERY_INFO_REPLY (Server -> Client)

```
"ROMP" 0x02
u8  hostnamelen, hostname
u8  motdlen, motd
u8  versionlen, version      // mis. "romp1"
u16 players, maxplayers
u8  has_password              // 1 = pakai password
```

### 0x03 QUERY_PLAYERS (Client -> Server)

```
"ROMP" 0x03
```

### 0x04 QUERY_PLAYERS_REPLY (Server -> Client)

```
"ROMP" 0x04
u8  count
per pemain: u16 playerid, u8 namelen, name
```

## Master Server (daftar server publik)

`romp-master` (lihat `server/src/romp_master.c`, default port 7778) menyimpan
daftar server yang announce. Game server mendaftarkan diri tiap 60 detik
(opsi `announce 1` + `master_host`/`master_port` di server.cfg). Entri yang
tidak di-announce ulang dalam 180 detik dihapus otomatis.

Protokol TCP, framing sama (u16 LE length + type):

### 0x50 ANNOUNCE (Game server -> Master)

```
u16 gameport, players, maxplayers
u8  hostnamelen, hostname
u8  motdlen, motd
```
IP server diambil dari koneksinya (getpeername).

### 0x51 LIST_REQUEST (Client -> Master)

Kosong (hanya type).

### 0x52 LIST (Master -> Client)

```
u16 count
per server:
  u8  iplen, ip
  u16 port, players, maxplayers
  u8  hostnamelen, hostname
  u8  motdlen, motd
```

Alur browser tipikal: `FetchMasterList` -> untuk tiap entri `QueryServer`
via UDP (info live + ping) -> tampilkan daftar -> user pilih -> connect.

## LAN Discovery (tanpa master/VPS)

Server yang `lan_discovery 1` (default) broadcast beacon UDP tiap 3 detik
ke `255.255.255.255:7779` supaya client di jaringan lokal yang sama
otomatis menemukannya — murni server -> client, tanpa perantara.

```
"ROMP" 0x11 LAN_BEACON
u16 gameport
u8  hostnamelen, hostname
u16 players, maxplayers
u8  has_password
u8  motdlen, motd
```

Client cukup listen di UDP 7779 dan kumpulkan beacon (lihat
`ROMPBrowser::ListenLan` / `BrowserListenLan`). Tidak perlu tahu IP
server sebelumnya. Cocok untuk main bareng satu WiFi/warnet.

## Alur koneksi tipikal

```
C -> S  HELLO("Andi")
S -> C  WELCOME(id=3, ...)
S -> C  PLAYER_LIST([...pemain online...])
S -> *  PLAYER_JOIN(3, "Andi")        // ke semua pemain lain
C -> S  POS ... (10 Hz)
S -> *  PLAYER_POS(3, ...)            // ke semua pemain lain
C -> S  CHAT("halo semua")
S -> *  CHAT_MSG(3, kind=0, "Andi (3): halo semua")
C -> S  CHAT("/q")
S -> *  PLAYER_LEAVE(3, 0)
```

## Batasan anti-spam

- Chat: maks 3 pesan per 2 detik, selebihnya pesan dibuang; spam berat = kick.
- POS: paket yang datang lebih cepat dari 30ms diabaikan.
- Nickname duplikat / invalid = REJECT.
