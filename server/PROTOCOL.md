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

### 0x12 VEH (Client -> Server)
Sync posisi kendaraan, dikirim ~10x per detik **oleh sopir saja**.
Paket dari non-sopir diabaikan server (anti-cheat dasar). Rate limit 33 Hz.

```
u16 vehicleid
float x, y, z
float heading
u16 health  // 0-1000, <=250 = hancur
```

### 0x13 VEH_SYNC
Broadcast posisi kendaraan (tidak di-echo ke pengirim paket 0x12).

```
u16 driverid  // 0xFFFF = update server-side (teleport/respawn), bukan dari sopir
u16 vehicleid
float x, y, z
float heading
u16 health
```

### 0x14 VEH_SPAWN
Kendaraan baru muncul (broadcast saat `romp.createVehicle`; dikirim juga ke
pemain yang baru join untuk semua kendaraan aktif).

```
u16 vehicleid
u16 model        // id model SA-MP 400-611
float x, y, z
float angle
u8  color1
u8  color2
u16 health
u16 driverid     // 0xFFFF = kosong
```

### 0x15 VEH_DESPAWN
Kendaraan dihancurkan (`romp.destroyVehicle`).

```
u16 vehicleid
```

### 0x16 VEH_SEAT
Perubahan penumpang (masuk/keluar/pindah kursi).

```
u16 vehicleid
u8  seat         // 0 = sopir, 1-3 = penumpang
u16 playerid     // 0xFFFF = kursi dikosongkan
```

API Lua (di-register C, `server/vehicle.lua` hanya untuk NUI):
`romp.createVehicle(model, x, y, z, angle, c1, c2, respawnDelayMs)` /
`romp.destroyVehicle(vid)` / `romp.setVehiclePos(vid, x, y, z [, angle])` /
`romp.getVehiclePos(vid)` / `romp.setVehicleHealth(vid, health [, killerid])` /
`romp.getVehicleHealth(vid)` / `romp.damageVehicle(vid, amount [, killerid])` /
`romp.putPlayerInVehicle(pid, vid, seat)` / `romp.removePlayerFromVehicle(pid)` /
`romp.isPlayerInVehicle(pid)` / `romp.getPlayerVehicle(pid)` /
`romp.getPlayerVehicleSeat(pid)` / `romp.getVehicleModel(vid)` /
`romp.getVehicleColors(vid)` / `romp.setVehicleColors(vid, c1, c2)` /
`romp.getVehicleDriver(vid)` / `romp.getVehiclePassenger(vid, seat)` /
`romp.respawnVehicle(vid)` / `romp.getVehicles()`.
Event: `onPlayerEnterVehicle(pid, vid, seat)`,
`onPlayerExitVehicle(pid, vid)`, `onVehicleDamage(vid, damagerid, newhealth)`,
`onVehicleDeath(vid, killerid)`, `onVehicleSpawn(vid)`.
Health <= 250 = hancur: penumpang dikeluarkan, event death, lalu respawn
otomatis ke posisi spawn setelah `respawnDelayMs` (0 = tidak respawn).

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
u8  flags   // bit0 = 1 kalau pemain ini NPC/bot (lihat "NPC/bot ala SA-MP")
```

> Client lama yang parse nama pakai `namelen` boleh mengabaikan byte
> tambahan ini — format lama tetap kompatibel.

### 0x23 PLAYER_LEAVE

### 0x23 PLAYER_LEAVE

```
u16 playerid
u8  reason  // 0 = quit, 1 = kick, 2 = timeout
```

### 0x24 PLAYER_LIST
Daftar pemain yang sedang online, dikirim tepat setelah WELCOME.
Hanya berisi pemain yang terlihat oleh penerima (vw & interior sama,
lihat "Interior & Virtual World" di bawah).

```
u8 count
// per pemain:
u16 playerid
u8  namelen
char name[namelen]
```

### 0x25 INT_VW
Dikirim server ke pemain setiap kali interior/virtual world-nya berubah
(via `romp.setPlayerInterior` / `romp.setPlayerVirtualWorld`). Client pakai
ini untuk tahu dunia mana yang sedang ia huni.

```
u8  interior
u32 virtual_world  // little-endian
```

## Interior & Virtual World (ala SA-MP)

Setiap pemain punya `interior` (0-255) dan `virtual world` (0-2^31-1),
default 0/0. Dua pemain saling **terlihat** hanya bila vw DAN interior
mereka sama. Yang di-scope:

- `0x11 PLAYER_POS`, chat pemain (`0x20`/`KIND_CHAT`, `/me`), `0x18 WPN_SYNC`
- `0x22 PLAYER_JOIN` / `0x23 PLAYER_LEAVE` (entity)
- `0x24 PLAYER_LIST` (hanya yang terlihat)

Pengumuman server (`romp.sendChatToAll`, join/kick) tetap global.
Saat pemain pindah vw/interior, server otomatis mengirim `0x25` ke pemain
tersebut plus `0x23`/`0x22` dua arah ke pemain yang (tak) terlihat lagi,
dan me-resync kendaraan (lihat bawah). Event Lua:
`onPlayerInteriorChange(pid, old, new)`,
`onPlayerVirtualWorldChange(pid, old, new)`.

Kendaraan juga punya virtual world (`romp.setVehicleVirtualWorld`,
default 0). `0x14 VEH_SPAWN` / `0x15 VEH_DESPAWN` / `0x16 VEH_SEAT` hanya
dikirim ke pemain yang vw-nya sama dengan kendaraan; `0x13 VEH_SYNC`
hanya ke pemain yang terlihat oleh sopir. Bila sopir pindah vw,
kendaraannya ikut pindah.

### 0x17 WPN_SHOT (Client -> Server)
Event tembakan dari pemain. Server bersifat otoritatif untuk inventory:
tembakan hanya diterima bila `weaponid` ada di inventory server pemain
(hasil `romp.giveWeapon`). Rate limit ~25 tembakan/detik; titik kena tidak
boleh lebih dari 400 unit dari posisi terakhir pemain (anti teleport-bullet).
Tembakan yang lolos di-relay sebagai `0x18 WPN_SYNC`.

```
u16 weaponid  // id senjata SA-MP 1..46
u8  hittype   // 0 = tidak kena, 1 = pemain, 2 = kendaraan, 3 = objek/dunia
u16 hitid     // playerid / vehicleid target, 0 bila hittype 0
float x, y, z // titik kena di dunia
```

Validasi target: `hittype=1` butuh playerid valid & bukan diri sendiri,
`hittype=2` butuh vehicleid valid.

### 0x18 WPN_SYNC (Server -> Client)
Broadcast tembakan (tidak di-echo ke penembak).

```
u16 shooterid
u16 weaponid
u8  hittype   // 0 = tidak kena, 1 = pemain, 2 = kendaraan, 3 = objek/dunia
u16 hitid
float x, y, z // titik kena di dunia
```

### 0x19 WPN_SET (Server -> Client)
Set slot senjata pemain (inventory otoritatif server). Dikirim ke pemain
itu saja saat `romp.giveWeapon` / `romp.removePlayerWeapon` /
`romp.resetPlayerWeapons`.

```
u8  slot      // 0..12, mapping ala SA-MP (lihat bawah)
u16 weaponid  // 0 = slot dikosongkan
u16 ammo
```

API Lua (di-register C): `romp.giveWeapon(pid, weaponid, ammo)` /
`romp.removePlayerWeapon(pid, weaponid)` / `romp.resetPlayerWeapons(pid)` /
`romp.getPlayerWeapon(pid)` (kembalikan weaponid, ammo, slot aktif) /
`romp.getPlayerWeaponData(pid, slot)` (kembalikan weaponid, ammo) /
`romp.weaponSlot(weaponid)` (kembalikan slot 0..12 atau -1).
Konstanta: `romp.WPN_HIT_NONE/PLAYER/VEHICLE/OBJECT` (0..3).
Event: `onPlayerWeaponShot(pid, weaponid, hittype, hitid, x, y, z)`
(mirip `OnPlayerWeaponShot` SA-MP; damage tidak dihitung server —
gamemode yang memutuskan).

Slot senjata ala SA-MP:
0: 0,1 (tangan/knuckle) | 1: 2-9 (melee) | 2: 22-24 (pistol) |
3: 25-27 (shotgun) | 4: 28,29,32 (smg) | 5: 30,31 (rifle) |
6: 33,34 (sniper) | 7: 35-38 (heavy) | 8: 16-18,39 (projectile) |
9: 14 (spraycan) | 10: 42-44 | 11: 45 | 12: 40 (detonator)

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

### Dialog ala SA-MP (via NUI)

`romp.showDialog(id, dialogId, style, caption, info, button1, button2)`
mengirim pesan berikut lewat 0x40, lalu 0x42 focus=1:

```json
{"action":"showDialog","id":100,"style":2,
 "caption":"Menu Demo","info":"Info Server\nGanti Nickname",
 "button1":"Pilih","button2":"Batal"}
```

`style`: 0=MSGBOX, 1=INPUT, 2=LIST, 3=PASSWORD. `info` boleh multi-baris
(`\n`); untuk LIST tiap baris jadi satu item yang bisa diketuk.

`romp.hideDialog(id)` mengirim `{"action":"hideDialog"}` lalu 0x42 focus=0.

Jawaban pemain dikirim client sebagai 0x41 NUI_CALLBACK bernama
`__dialog_response` dengan JSON:

```json
{"dialogId":100,"response":true,"listitem":1,"inputtext":"Ganti Nickname"}
```

`response`: true = tombol1 / item di-klik, false = tombol2 / ditutup.
`listitem`: index item (0-based) untuk LIST, -1 untuk style lain.
`inputtext`: teks input (INPUT/PASSWORD) atau teks item yang di-klik (LIST).

Server me-routing otomatis ke event Lua
`onDialogResponse(id, name, dialogId, response, listitem, inputtext)`
(lihat `server/dialog.lua`). Halaman UI referensi:
`android/app/src/main/assets/romp/nui/dialog.html`.

### Textdraw ala SA-MP (via NUI)

API Lua di `server/textdraw.lua` (dimuat otomatis seperti `dialog.lua`):
global ala `TextDraw*` (`romp.createTextDraw/showTextDraw/hideTextDraw/`
`showTextDrawForPlayer/hideTextDrawForPlayer/setTextDrawString/`
`setTextDrawStyle/destroyTextDraw/isTextDrawVisible`) dan per-pemain ala
`PlayerTextDraw*` (`romp.createPlayerTextDraw/showPlayerTextDraw/`
`hidePlayerTextDraw/setPlayerTextDrawString/setPlayerTextDrawStyle/`
`destroyPlayerTextDraw`). Koordinat SA-MP 640x480.

Pesan 0x40 yang dikirim (id `"g1"` = global, `"p0_1"` = pemain 0):

```json
{"action":"showTextDraw","id":"g1","text":"RO-MP SERVER","x":320,"y":16,
 "color":4294967295,"size":2.2,"align":2,"font":1,
 "box":1,"boxColor":136,"outline":1,"shadow":0}
{"action":"setTextDrawString","id":"g1","text":"Teks baru"}
{"action":"hideTextDraw","id":"g1"}
```

`color`/`boxColor`: 0xRRGGBBAA. `align`: 1=kiri, 2=tengah, 3=kanan.
Textdraw global yang sedang tampil otomatis dikirim ulang ke pemain yang
baru join; textdraw per-pemain dibersihkan saat pemain leave.
Halaman UI referensi: `android/app/src/main/assets/romp/nui/textdraw.html`.

### Checkpoint ala SA-MP (via NUI)

API Lua di `server/checkpoint.lua` (dimuat otomatis seperti `dialog.lua`).
Normal: `romp.createCheckpoint(romp.CHECKPOINT_NORMAL, x, y, z, size)`.
Race: `romp.createCheckpoint(romp.CHECKPOINT_RACE, x, y, z, size)`.
Satu checkpoint aktif per pemain ala SA-MP (`romp.showCheckpointForPlayer`/
`romp.hideCheckpointForPlayer/getPlayerCheckpoint`); tambah
`romp.destroyCheckpoint/setCheckpointPos/setRaceCheckpointNext/`\n`romp.getCheckpointPos/isPlayerInCheckpoint` dan `romp.getPlayerPos(id)`
(posisi dunia terakhir dari paket 0x10, `nil` jika belum ada).

Server mendeteksi masuk/keluar tiap detik (event `onTick`) dan memanggil
event Lua mirip SA-MP: `onPlayerEnterCheckpoint(pid, cpid)` /
`onPlayerLeaveCheckpoint(pid, cpid)`; varian race:
`onPlayerEnterRaceCheckpoint(pid, cpid)` / `onPlayerLeaveRaceCheckpoint(pid, cpid)`.

Pesan 0x40 yang dikirim (id `"c1"` = checkpoint 1):

```json
{"action":"showCheckpoint","id":"c1","type":0,
 "x":100.0,"y":200.0,"z":3.0,"size":5.0,
 "nextX":150.0,"nextY":200.0,"nextZ":3.0}
{"action":"hideCheckpoint","id":"c1"}
```

`type`: 0=normal, 1=race. `nextX/Y/Z` opsional (hanya race).
Checkpoint bersifat per-pemain, jadi tidak ada resync saat join;
state dibersihkan saat pemain leave.
Halaman UI referensi: `android/app/src/main/assets/romp/nui/checkpoint.html`.

### Pickup (NUI_MESSAGE `0x40`)

```json
{"action":"showPickup","id":"p1","model":1242,"ptype":0,
 "x":100.0,"y":200.0,"z":3.0}
{"action":"hidePickup","id":"p1"}
```

`ptype`: 0=respawn setelah diambil, 1=hilang setelah diambil (once),
2=tetap tampil, event tiap masuk radius. Event Lua `onPlayerPickup(pid,
pickupid)` dipicu server-side (deteksi tiap detik via `onTick`,
radius 1.5). State pickup bersifat per-pemain (show/hide/respawn
untuk tiap pemain), dibersihkan saat pemain leave.
Halaman UI referensi: `android/app/src/main/assets/romp/nui/pickup.html`.

### Map icon / radar blip (NUI_MESSAGE `0x40`)

```json
{"action":"setMapIcon","id":"mi5","iconid":5,
 "x":100.0,"y":200.0,"z":3.0,"markertype":2,
 "color":4278190335,"style":1}
{"action":"removeMapIcon","id":"mi5"}
{"action":"playerPos","x":98.0,"y":199.0,"z":3.0}
```

API Lua di `server/mapicon.lua` (dimuat otomatis seperti `dialog.lua`),
mirip SA-MP `SetPlayerMapIcon`/`RemovePlayerMapIcon`:
`romp.setPlayerMapIcon(pid, iconid, x, y, z, markertype, color, style)` /
`romp.removePlayerMapIcon(pid, iconid)` / `romp.removeAllPlayerMapIcons(pid)`
/ `romp.getPlayerMapIcon(pid, iconid)`.

`iconid`: 0-99 per pemain. `markertype`: 1=local, 2=global, 3=local
checkpoint, 4=global checkpoint. `color`: 0xRRGGBBAA. `style`: 1=permanen,
2=berkedip. Konstanta: `romp.MAPICON_LOCAL/GLOBAL/LOCAL_CHECKPOINT/`
`GLOBAL_CHECKPOINT` dan `romp.MAPICON_STYLE_PERMANENT/FLASHING`.
Server mengirim `playerPos` tiap detik (via `onTick`) ke pemain yang punya
icon agar radar UI bisa menggambar blip relatif terhadap pemain; state
dibersihkan saat pemain leave.
Halaman UI referensi: `android/app/src/main/assets/romp/nui/mapicon.html`.

### Kendaraan ala SA-MP (NUI_MESSAGE `0x40`)

```json
{"action":"showVehicle","id":"v1","vid":1,"model":411,
 "x":100.0,"y":200.0,"z":3.0,"angle":90.0,"c1":0,"c2":0,
 "health":1000,"destroyed":false,"driver":3,"seats":"3,-,-,-"}
{"action":"updateVehicle","id":"v1","vid":1,"x":101.0,"y":200.0,"z":3.0,
 "health":800,"destroyed":false,"driver":3,"seats":"3,-,-,-"}
{"action":"hideVehicle","id":"v1"}
```

Dikirim oleh `server/vehicle.lua` (dimuat otomatis), di-diff tiap detik dari
`romp.getVehicles()`. `seats`: daftar penghuni kursi 0-3 dipisah koma
(`-` = kosong). State otoritatif kendaraan ada di server C
(`server/src/romp_vehicle.c`); client game merender kendaraan 3D dari paket
`0x13`/`0x14`/`0x15`/`0x16`, halaman ini hanya indikator 2D fallback.
Halaman UI referensi: `android/app/src/main/assets/romp/nui/vehicle.html`.

## Filterscript ala SA-MP (modul Lua terpisah)

Mirip SA-MP: selain gamemode utama, server bisa me-load **filterscript** —
modul Lua terpisah yang bisa di-load/unload **tanpa restart server**.
Tiap filterscript punya **state Lua sendiri** (variabel global terpisah dari
gamemode & filterscript lain) tapi API `romp.*` yang sama, termasuk helper
yang dimuat otomatis (`dialog.lua`, `textdraw.lua`, `checkpoint.lua`,
`pickup.lua`, `mapicon.lua` — dicari di folder filterscript dulu, lalu
fallback ke folder gamemode).

**Lokasi file:** `<folder gamemode>/filterscripts/<nama>.lua`
(nama polos, tanpa `/`, `\`, atau `..` — anti path traversal).

**Konfigurasi** (`server.cfg`): `filterscripts contoh` — daftar nama dipisah
spasi, di-load otomatis saat server start (setelah gamemode).

**Console & RCON** (command sama, RCON otomatis ikut karena satu `console_run`):

```
loadfs <nama>     - load filterscripts/<nama>.lua
unloadfs <nama>   - unload (panggil onFilterScriptExit dulu)
reloadfs <nama>   - unload + load ulang
filterscripts     - daftar filterscript yang sedang aktif
```

**Urutan event** (ala SA-MP): filterscript dipanggil **DULU** (urutan load),
baru gamemode. Kalau ada yang `return true` (= handled), sisanya dilewati —
contoh: filterscript bisa "mencuri" command sebelum gamemode melihatnya.
Callback NUI (`romp.onNUICallback`) dikirim ke **semua** modul yang
mendaftarkan nama itu (mirip `OnDialogResponse` SA-MP yang dipanggil di tiap
script; tiap script cek id-nya sendiri).

**Lifecycle per modul:**

```lua
function onFilterScriptInit()  -- dipanggil sekali saat load
    romp.log("halo dari filterscript!")
end

function onFilterScriptExit()  -- dipanggil sekali saat unload
    romp.log("dadah!")
end
```

**API Lua:**

```lua
romp.loadFilterScript("nama")    -- true = dimuat / sudah dimuat
romp.unloadFilterScript("nama")  -- true = berhasil di-unload
romp.getFilterScripts()          -- {"contoh", "anticheat", ...}
```

Catatan: pemain yang sudah online **tidak** menerima `onPlayerJoin` ulang
saat filterscript di-load belakangan (sama seperti SA-MP). Maksimal 16
filterscript aktif bersamaan. Contoh: `server/filterscripts/contoh.lua`.

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

## RCON — remote console ala SA-MP

Admin bisa menjalankan command console server dari jauh (mis. dari PC
rumah) tanpa akses ke mesin server, lewat **UDP di port game yang sama**.
Butuh `rcon 1` + `rcon_password <password>` di server.cfg — tanpa password,
RCON mati total (tidak dijawab). Command yang tersedia sama persis dengan
console lokal: `help`, `say`, `players`, `vehicles`, `kick`, `ban`,
`unban`, `bans`, `stop` (plus command custom gamemode via
`onConsoleCommand`). Tool siap pakai: `server/rcon.py`.

### 0x05 RCON (Client -> Server)

```
"ROMP" 0x05
u8  pwlen
char pw[pwlen]       // password RCON (plain, seperti SA-MP)
u16 cmdlen           // little-endian
char cmd[cmdlen]     // baris command, mis. "kick 3 spam"
```

### 0x06 RCON_REPLY (Server -> Client)

```
"ROMP" 0x06
u8  status   // 0 = OK, 1 = password salah / RCON mati
u16 outlen   // little-endian
char out[outlen]  // output teks command, baris dipisah '\n'
```

Anti brute-force: maks 2 request/detik per IP (lebihnya diabaikan diam);
5x password salah beruntun = IP diblokir 60 detik. Setiap command yang
dieksekusi dicatat di log server (`[rcon] <ip>: <command>`).

## NPC/bot ala SA-MP (ConnectNPC)

Koneksi virtual untuk testing & populasi server — mirip `ConnectNPC` di
SA-MP. NPC **tidak punya socket TCP**: tak mengirim/menerima paket apa pun,
tak kena timeout, tak di-pump loop select, tapi menempati slot pemain
(`client_count`) dan muncul di semua tempat seperti pemain biasa:
- `0x24 PLAYER_LIST` & `0x03/0x04` UDP query (dihitung sebagai pemain)
- `0x22 PLAYER_JOIN` dengan `flags` bit0 = 1 (client bisa tampilkan ikon
  "[NPC]" / warna berbeda)
- `0x23 PLAYER_LEAVE` saat disconnect; chat pemain (NPC bisa chat via
  gamemode); `/players` di console menandai `[NPC]`

Posisi NPC di-relay seperti pemain biasa lewat `0x11 PLAYER_POS`
(digagas gamemode, mis. tiap detik), jadi NPC bisa "berjalan" di dunia.
NPC ikut aturan visibility vw & interior: relay & JOIN/LEAVE hanya ke
pemain yang se-vw & se-interior.

### API Lua

```
romp.connectNPC(name)            -- -> id | nil, err (validasi nick ala SA-MP,
                                 --    nama tak boleh sudah dipakai, slot penuh = gagal)
romp.disconnectNPC(id[, reason]) -- -> bool; reason: 0=quit, 1=kick, 2=timeout
romp.isNPC(id)                   -- -> bool
romp.setNPCPos(id, x, y, z)      -- -> bool; relay 0x11 ke pemain yang melihat
romp.npcSendChat(id, text)       -- -> bool; memicu onPlayerChat dulu ala
                                 --    SA-MP (NPC chat memicu OnPlayerText)
romp.getNPCs()                   -- -> tabel array berisi id NPC yang online
romp.getPlayers()                -- tiap entri punya field boolean `isnpc`
```

Event Lua yang menyala untuk NPC sama seperti pemain biasa
(`onPlayerJoin`, `onPlayerLeave`, `onPlayerChat`, `onPlayerCommand`,
`onPlayerInteriorChange`, `onPlayerVirtualWorldChange`) — gamemode
membedakan via `romp.isNPC(id)`.

### Console & RCON

```
npc <nama>    - sambungkan NPC/bot (koneksi virtual)
unnpc <id>    - putuskan NPC/bot
players       - daftar online: "[id] nama [NPC]" + hitung NPC terpisah
```

Kick via console/RCON (`kick <id>`) juga menangani NPC dengan benar
(mengarahkan ke `npc_disconnect`).

### Batasan (v1)

- NPC **belum bisa naik kendaraan** (`romp.putPlayerInVehicle` untuk NPC
  selalu gagal) — direncanakan di iterasi berikut.
- NPC tak bisa menerima NUI (0x40) — tak ada WebView di sisi bot.
- Fix penting: loop select server melewatkan NPC saat `FD_SET` (sebelumnya
  `FD_SET(-1)` pada sock NPC = undefined behavior / korupsi memori).

Test: `~/workspace/romp/tests/test_npc.py`
(+ `test_npc_gamemode.lua`) — 34 checks.

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
S -> C  VEH_SPAWN(...)                // tiap kendaraan aktif (stream saat join)
S -> *  PLAYER_JOIN(3, "Andi")        // ke semua pemain lain
C -> S  POS ... (10 Hz)
S -> *  PLAYER_POS(3, ...)            // ke semua pemain lain
S -> *  VEH_SPAWN(1, ...)             // Lua: romp.createVehicle
S -> *  VEH_SEAT(1, seat=0, pid=3)    // Lua: romp.putPlayerInVehicle
C -> S  VEH(1, x,y,z, heading, hp)    // sopir sync ~10 Hz
S -> *  VEH_SYNC(3, 1, ...)           // ke semua pemain lain
C -> S  CHAT("halo semua")
S -> *  CHAT_MSG(3, kind=0, "Andi (3): halo semua")
C -> S  CHAT("/q")
S -> *  PLAYER_LEAVE(3, 0)
```

## Batasan anti-spam

- Chat: maks 3 pesan per 2 detik, selebihnya pesan dibuang; spam berat = kick.
- POS: paket yang datang lebih cepat dari 30ms diabaikan.
- VEH: hanya sopir yang boleh sync; paket non-sopir diabaikan; rate limit 30ms.
- Nickname duplikat / invalid = REJECT.
