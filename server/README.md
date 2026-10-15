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
| `database` | *(kosong)* | Path file SQLite gamemode; kosong = fitur DB mati |
| `gravity` | 0.008 | Gravitasi awal dunia (ala SA-MP `SetGravity`) |
| `weather` | 10 | Cuaca awal dunia (ala SA-MP `SetWeather`; 10 = Sunny) |
| `worldtime` | 12:00 | Jam awal dunia (ala SA-MP `SetWorldTime`), format `HH:MM` |

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

**Timer ala SA-MP (`SetTimer`/`SetTimerEx`/`KillTimer`):**

| Fungsi | Keterangan |
|---|---|
| `romp.setTimer(cb, ms, repeating, ...)` | `cb` = fungsi Lua atau nama fungsi global; dipanggil tiap `ms` (min 50). Argumen tambahan diteruskan ke callback ala `SetTimerEx`. → timerid / `nil, errmsg` |
| `romp.killTimer(timerid)` | Hentikan timer → `true`/`false` |
| `romp.getTimerCount()` | Jumlah timer aktif |

```lua
local tid = romp.setTimer("updateSkor", 60000, true)     -- tiap 60 dtk
romp.setTimer(function(pesan) romp.sendChatToAll(romp.KIND_INFO, pesan) end,
              5000, false, "Halo!")                      -- sekali, bawa argumen
romp.killTimer(tid)
```

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

**3D text label ala SA-MP (`Create3DTextLabel`):**

| Fungsi | Keterangan |
|---|---|
| `romp.create3DTextLabel(text, color, x, y, z, drawDist [, vw])` | Buat label statis → labelid / nil. `color` = `0xRRGGBBAA`, `drawDist` <= 0 = default 100 |
| `romp.delete3DTextLabel(id)` | Hapus label → `true`/`false` |
| `romp.update3DTextLabelText(id, color, text)` | Ubah teks & warna → `true`/`false` |
| `romp.set3DTextLabelPos(id, x, y, z)` | Pindahkan label statis → `true`/`false` (gagal bila di-attach) |
| `romp.attach3DTextLabelToPlayer(id, pid [, ox, oy, oz])` | Tempel ke pemain (default offset di atas kepala) → `true`/`false` |
| `romp.attach3DTextLabelToVehicle(id, vid [, ox, oy, oz])` | Tempel ke kendaraan → `true`/`false` |
| `romp.get3DTextLabels()` | Array `{ {id, text, color, x, y, z, drawdist, vw, attach={type, pid/vid, ox, oy, oz}}, ... }` |
| `romp.LABEL_STATIC` / `LABEL_PLAYER` / `LABEL_VEHICLE` | Konstanta `attach.type` (0/1/2) |

Label yang di-attach ikut terhapus saat pemain disconnect / kendaraan
dihancurkan (ala SA-MP), dan ikut pindah virtual world bersama entity-nya.
Paket: `0x28` LABEL_SPAWN / `0x29` LABEL_DESPAWN / `0x2A` LABEL_TEXT /
`0x2B` LABEL_POS (lihat `PROTOCOL.md`).

```lua
-- Contoh: papan nama melayang di atas pemain.
local lid = romp.create3DTextLabel("Toko Bang Jago", 0xFFFF00FF, 100.0, 200.0, 3.0, 80.0)
romp.attach3DTextLabelToPlayer(lid, playerid)  -- mengikuti pemain
romp.update3DTextLabelText(lid, 0x00FF00FF, "Toko Bang Jago [BUKA]")
```

**Objek ala SA-MP (`CreateObject` / `MoveObject`):**

| Fungsi | Keterangan |
|---|---|
| `romp.createObject(modelid, x, y, z, rx, ry, rz [, drawDist])` | Buat objek → objid / nil. `drawDist` <= 0 = default 300 |
| `romp.destroyObject(id)` | Hapus objek → `true`/`false` |
| `romp.isValidObject(id)` | `true` bila objek ada |
| `romp.moveObject(id, x, y, z, speed [, rx, ry, rz])` | Gerakkan ke target (`speed` > 0 unit/detik; rotasi `-1000` = tetap) → `true`/`false` |
| `romp.stopObject(id)` | Hentikan gerakan → `true`/`false` |
| `romp.isObjectMoving(id)` | `true` bila sedang bergerak |
| `romp.getObjectPos(id)` | `x, y, z` / nil |
| `romp.setObjectPos(id, x, y, z)` | Set posisi instan (hentikan gerakan) → `true`/`false` |
| `romp.getObjectRot(id)` | `rx, ry, rz` / nil |
| `romp.setObjectRot(id, rx, ry, rz)` | Set rotasi instan → `true`/`false` |
| `romp.getObjectModel(id)` | modelid / nil |
| `romp.getObjectVirtualWorld(id)` | vw / nil (`-1` = semua world) |
| `romp.setObjectVirtualWorld(id, vw)` | Pindah vw (`-1` = semua world) → `true`/`false` |
| `romp.getObjects()` | Array `{ {id, modelid, x, y, z, rx, ry, rz, drawdist, vw, moving}, ... }` |
| `romp.OBJECT_NO_ROT` | Konstanta `-1000.0` (sentinel rotasi "tak berubah" ala SA-MP) |

**Event:** `onObjectMoved(objid)` — menyala saat `MoveObject` tiba di target
(ala SA-MP `OnObjectMoved`).

Objek di-scope virtual world (`vw` objek `-1` atau == vw pemain); pemain yang
join di tengah gerakan menerima posisi saat ini + sisa gerakan. Paket:
`0x2C` OBJ_SPAWN / `0x2D` OBJ_DESPAWN / `0x2E` OBJ_MOVE / `0x2F` OBJ_UPDATE
(lihat `PROTOCOL.md`).

```lua
-- Contoh: gerbang yang terbuka saat pemain mengetik /buka.
local gate = romp.createObject(980, 100.0, 200.0, 10.0, 0.0, 0.0, 90.0)
function onObjectMoved(objid)
    romp.log("objek " .. objid .. " tiba di target")
end
romp.moveObject(gate, 100.0, 210.0, 10.0, 5.0)  -- geser 10 unit, 5 unit/detik
```

**Actor statis ala SA-MP (`CreateActor`):**

Actor = ped statis server-side, lebih ringan dari NPC (`ConnectNPC`):
tidak menempati slot pemain, tidak punya koneksi, dan posisinya hanya
berubah lewat Lua. Damage dihitung **server** dari `weaponid` (tabel ala
GTA:SA) saat client melaporkan tembakan `0x17` dengan `hittype = 4`
(`romp.WPN_HIT_ACTOR`).

| Fungsi | Keterangan |
|---|---|
| `romp.createActor(modelid, x, y, z, angle)` | Buat actor → actorid / nil (`modelid` = skin 0-311) |
| `romp.destroyActor(id)` | Hapus actor → `true`/`false` |
| `romp.isValidActor(id)` | `true` bila actor ada |
| `romp.setActorPos(id, x, y, z)` | Pindah posisi instan → `true`/`false` |
| `romp.getActorPos(id)` | `x, y, z` / nil |
| `romp.setActorFacingAngle(id, angle)` | Set facing angle → `true`/`false` |
| `romp.getActorFacingAngle(id)` | angle / nil |
| `romp.setActorHealth(id, health)` | Set health 0-100 (tidak memicu `onActorDeath`) → `true`/`false` |
| `romp.getActorHealth(id)` | health / nil |
| `romp.setActorInvulnerable(id, bool)` | Kebal damage → `true`/`false` |
| `romp.isActorInvulnerable(id)` | `true`/`false` / nil |
| `romp.applyActorAnimation(id, anim)` | Terapkan animasi (mis. `"DANCING:DNCE_M_a"`) → `true`/`false` |
| `romp.clearActorAnimations(id)` | Bersihkan animasi → `true`/`false` |
| `romp.getActorVirtualWorld(id)` | vw / nil |
| `romp.setActorVirtualWorld(id, vw)` | Pindah vw → `true`/`false` |
| `romp.getActors()` | Array `{ {id, modelid, x, y, z, angle, health, invulnerable, vw, anim}, ... }` |

**Event:** `onPlayerGiveDamageActor(pid, actorid, amount, weaponid, bodypart)`
(ala SA-MP `OnPlayerGiveDamageActor`; `bodypart` selalu 0 karena client
tidak melaporkannya) dan `onActorDeath(actorid, killerid)`
(ala SA-MP `OnActorDeath`) — menyala saat damage membuat health mencapai 0.

Actor di-scope virtual world (hanya dikirim ke pemain yang se-vw).
Paket: `0x32` ACTOR_SPAWN / `0x33` ACTOR_DESPAWN / `0x34` ACTOR_POS /
`0x35` ACTOR_ANIM / `0x36` ACTOR_HEALTH (lihat `PROTOCOL.md`).

```lua
-- Contoh: penjaga toko yang kebal + joget.
local guard = romp.createActor(7, 100.0, 200.0, 3.0, 180.0)
romp.setActorInvulnerable(guard, true)
romp.applyActorAnimation(guard, "DANCING:DNCE_M_a")
function onPlayerGiveDamageActor(pid, aid, amount, wid, bodypart)
    romp.log("pemain " .. pid .. " menembak actor " .. aid ..
             " (" .. amount .. " dmg)")
end
function onActorDeath(aid, killerid)
    romp.sendChatToAll(romp.KIND_INFO,
        "*** Actor " .. aid .. " dibunuh " .. killerid .. ".")
end
```

**Audio stream ala SA-MP (`PlayAudioStreamForPlayer`):**

Server mengirim URL audio ke satu pemain; client memutarnya sendiri
(WebView/HTML5 audio atau MediaPlayer — klien Godot `pc-game/` sudah
memutar via `AudioStreamPlayer` setelah mengunduh URL-nya). Satu stream
per pemain: URL baru otomatis menggantikan yang lama (implicit stop).
Tidak di-broadcast dan tidak di-scope virtual world. State dibersihkan
saat pemain keluar. Maks 255 byte URL (dipotong bila lebih panjang).

| API | Keterangan |
|-----|------------|
| `romp.playAudioStreamForPlayer(pid, url [, x, y, z, distance, usepos, loop])` | Putar URL audio → `true`/`false`. `usepos=1` = posisional 3D di `(x, y, z)` dengan jarak dengar `distance` (default 50.0 ala SA-MP); `loop=1` = ulang terus. |
| `romp.stopAudioStreamForPlayer(pid)` | Hentikan stream → `true` bila ada stream yang dihentikan, `false` bila tidak ada |
| `romp.getPlayerAudioStream(pid)` | `{url, x, y, z, distance, usepos, loop}` / nil |

Paket: `0x37` AUDIO_PLAY / `0x38` AUDIO_STOP (lihat `PROTOCOL.md`).
Tak ada event (audio bersifat pasif, seperti di SA-MP).

```lua
-- Contoh: musik lobby global + efek posisional di pintu masuk.
function onPlayerJoin(id, name)
    romp.playAudioStreamForPlayer(id, "https://contoh.test/lobby.ogg",
                                 0, 0, 0, 50.0, 0, 1)  -- loop
end
-- /audiofx: bunyi bel di posisi pemain, jarak dengar 30 unit.
romp.playAudioStreamForPlayer(id, "https://contoh.test/bel.mp3",
                              x, y, z, 30.0, 1, 0)
```

**Attached object ala SA-MP (`SetPlayerAttachedObject` /
`RemovePlayerAttachedObject`):**

Tempelkan objek di bone pemain (topi, kacamata, senjata di punggung,
dsb). Maks 10 slot per pemain (`romp.ATTACH_MAX_SLOTS`), index 0..9 —
set ulang slot yang sama menimpa (ala SA-MP). Bone 1..18 ala SA-MP
(1=spine, 2=head, 5/6=tangan, 17=neck, 18=jaw). Di-broadcast scope
virtual world ke semua pemain di vw pemilik, termasuk pemiliknya
sendiri. State di-resync saat pemain join / pindah vw, dibersihkan
saat pemain keluar.

| API | Keterangan |
|-----|------------|
| `romp.setPlayerAttachedObject(pid, index, modelid, bone, offx, offy, offz, rotx, roty, rotz, scalex, scaley, scalez)` | Tempelkan objek → `true`/`false`. `index` 0-9, `modelid` != 0, `bone` 1-18, tiap sumbu `scale` > 0. |
| `romp.removePlayerAttachedObject(pid, index)` | Lepaskan objek → `true` bila slot terisi, `false` bila slot kosong/invalid. |
| `romp.isPlayerAttachedObjectSlotUsed(pid, index)` | `true` bila slot terpakai. |
| `romp.getPlayerAttachedObjects(pid)` | Array `{ {index, modelid, bone, off={x,y,z}, rot={x,y,z}, scale={x,y,z}}, ... }` / nil. |
| `romp.ATTACH_MAX_SLOTS` | Konstanta = 10. |

Paket: `0x3F` ATTACH_OBJ (set 44 byte / remove 7 byte, lihat `PROTOCOL.md`).
Tak ada event (attach bersifat pasif, seperti di SA-MP).

```lua
-- /attach: tempelkan objek model 18632 di tangan kanan (bone 6).
romp.setPlayerAttachedObject(id, 0, 18632, 6,
    0.1, 0.2, 0.3,   -- offset dari bone
    0, 0, 90,        -- rotasi (derajat)
    1, 1, 1)         -- skala
-- /detach: lepaskan lagi.
romp.removePlayerAttachedObject(id, 0)
```

### Weapon skill ala SA-MP (`SetPlayerSkillLevel` / `GetPlayerSkillLevel`)

Level skill senjata per pemain (mempengaruhi handling senjata client:
akurasi, kecepatan bidik, dsb — seperti di SA-MP). 11 skill id 0..10
ala SA-MP (`WEAPONSKILL_*`): 0=Pistol, 1=Silenced, 2=Desert Eagle,
3=Shotgun, 4=Sawnoff, 5=Spas12, 6=Micro Uzi, 7=MP5, 8=AK47, 9=M4,
10=Sniper. Level 0..1000; pemain baru (termasuk NPC) mulai dari
`romp.SKILL_DEFAULT_LEVEL` (1). State otoritatif server, bersifat
per-pemain: paket `0x43` SKILL_SET dikirim hanya ke pemiliknya (tidak
di-broadcast, tanpa scope vw). Tak perlu resync saat join (selalu mulai
dari default); state di-reset ke default saat pemain keluar.

| API | Keterangan |
|-----|------------|
| `romp.setPlayerSkillLevel(pid, skill, level)` | Set level skill → `true`/`false`. `skill` 0-10, `level` 0-1000. |
| `romp.getPlayerSkillLevel(pid, skill)` | Level skill saat ini (int) / nil bila pemain/skill tak valid. |
| `romp.getPlayerSkillLevels(pid)` | Array 11 level `{[1]=level skill 0, ..., [11]=level skill 10}` / nil. |
| `romp.SKILL_PISTOL` .. `romp.SKILL_SNIPERRIFLE` | Konstanta id skill 0..10. |
| `romp.SKILL_COUNT` / `romp.SKILL_DEFAULT_LEVEL` | Konstanta = 11 / = 1. |

Paket: `0x43` SKILL_SET (frame 5 byte: pid, skill, u16 level — lihat `PROTOCOL.md`).
Tak ada event (skill bersifat pasif, seperti di SA-MP).

```lua
-- /skill: naikkan skill M4 ke 800.
romp.setPlayerSkillLevel(id, romp.SKILL_M4, 800)
-- /skills: tampilkan semua level.
local t = romp.getPlayerSkillLevels(id)
```

### Player marker ala SA-MP (`ShowPlayerMarkers` / `SetPlayerMarkerForPlayer` / `GetPlayerColor`)

Warna marker radar per pemain (0xRRGGBBAA), otomatis dari palet 16 warna
berdasar id saat connect (termasuk NPC) — seperti SA-MP yang memberi
warna otomatis. Gamemode bisa mengganti warna sendiri
(`romp.setPlayerColor`) atau menimpa warna pemain lain menurut pandangan
satu viewer saja (`romp.setPlayerMarkerForPlayer` — override disimpan
per-viewer di server, bukan global).

Mode marker global ala `ShowPlayerMarkers`: 0=off (tak ada marker yang
dikirim), 1=global, 2=streamed (default; sama dengan global di RO-MP
karena belum ada streaming berbasis jarak).

| API | Keterangan |
|-----|------------|
| `romp.showPlayerMarkers(mode)` | Set mode marker global → `true`/`false`. Broadcast `0x45`; bila dinyalakan kembali, warna dikirim ulang. |
| `romp.getPlayerMarkersMode()` | Mode saat ini (0/1/2). |
| `romp.setPlayerColor(pid, color)` | Ganti warna marker milik pemain (0xRRGGBBAA) → `true`/`false`. Broadcast `0x44` vw-scoped. |
| `romp.getPlayerColor(pid)` | Warna marker milik pemain (int) / nil bila pid tak valid. Ala SA-MP `GetPlayerColor`. |
| `romp.setPlayerMarkerForPlayer(viewer, showpid, color)` | Override: `viewer` melihat `showpid` dengan `color` → `true`/`false`. `0x44` hanya ke viewer. |
| `romp.clearPlayerMarkerForPlayer(viewer, showpid)` | Hapus override → `true` bila ada yang dihapus. |
| `romp.getPlayerMarkerForPlayer(viewer, showpid)` | Warna marker *efektif* (override / warna target) / nil. |
| `romp.MARKER_MODE_OFF` / `GLOBAL` / `STREAMED` | Konstanta mode = 0 / 1 / 2. |

Paket: `0x44` MARKER_SET (frame 6 byte: pid + u32 color LE — warna
efektif per-viewer, vw-scoped) & `0x45` MARKER_MODE (frame 2 byte: mode,
broadcast global) — lihat `PROTOCOL.md`. Resync otomatis saat join,
pindah interior/vw, dan saat mode dinyalakan. Tak ada event (marker
bersifat pasif, seperti di SA-MP).

```lua
-- /markers: ganti mode marker.
romp.showPlayerMarkers(romp.MARKER_MODE_GLOBAL)
-- /pcolor: warnai marker sendiri merah.
romp.setPlayerColor(id, 0xFF0000FF)
-- /pmark: menurut id, marker pemain 3 berwarna hijau.
romp.setPlayerMarkerForPlayer(id, 3, 0x00FF00FF)
```

### Kill feed ala SA-MP (`SendDeathMessage` / `OnPlayerDeath`)

Client melaporkan kematian pemain lokalnya via paket `0x46 DEATH_REPORT`
(u8 `killerid`, u8 `reason`); server memvalidasi lalu broadcast
`0x47 DEATH_MSG` (u8 `killerid` + u8 `victimid` + u8 `reason`) ke SEMUA
pemain (kill feed global ala SA-MP, tanpa scope vw/interior) + menyalakan
event Lua `onPlayerDeath(victimid, killerid, reason)`.

`killerid` = 255 (`romp.DEATH_KILLER_NONE`) berarti tak ada pembunuh
(jatuh/ledakan/tenggelam sendiri — ala SA-MP `INVALID_PLAYER_ID`).
`reason` = id senjata ala SA-MP (1..46) atau kode khusus (49=crash,
50=heliblades, 51=explosion, 53=drown, 54=splat).

Gamemode juga bisa broadcast kill feed manual via
`romp.sendDeathMessage(killerid, victimid, weaponid)` — ala SA-MP fungsi
ini TIDAK menyalakan event `onPlayerDeath`. Kematian NPC (tak punya
client untuk melapor) hanya bisa dimunculkan lewat cara manual ini.

| API | Keterangan |
|-----|------------|
| `romp.sendDeathMessage(killerid, victimid, weaponid)` | Broadcast kill feed manual → `true`/`false`. `0x47` global; tanpa event. |
| `romp.DEATH_KILLER_NONE` | Konstanta killerid tak ada pembunuh (= 255). |

Event: `onPlayerDeath(victimid, killerid, reason)` — hanya untuk laporan
client `0x46` (bukan untuk `sendDeathMessage` manual).

```lua
-- /deathmsg: kirim kill feed manual (killer 255 = tak ada pembunuh).
romp.sendDeathMessage(255, id, 51)

-- Event: umumkan kematian ke semua pemain.
function onPlayerDeath(victimid, killerid, reason)
    local v = romp.getPlayerName(victimid) or ("id " .. victimid)
    if killerid == 255 then
        romp.sendChatToAll(romp.KIND_SYS, "*** " .. v .. " mati.")
    else
        local k = romp.getPlayerName(killerid) or ("id " .. killerid)
        romp.sendChatToAll(romp.KIND_SYS,
            "*** " .. k .. " membunuh " .. v .. ".")
    end
end
```

Paket: `0x46` DEATH_REPORT (frame 3 byte, client → server, validasi
killerid connect/`reason` 0..60/payload pas 2 byte — tak valid dibuang
diam-diam) & `0x47` DEATH_MSG (frame 4 byte, server → semua client) —
lihat `PROTOCOL.md`. Pemain yang join belakangan tidak menerima riwayat
kill feed. Client C++: `IROMPPlatform::OnDeathMessage` +
`Client::SendDeathReport`; Godot: sinyal `death_msg`.

### Wanted level ala SA-MP (`SetPlayerWantedLevel` / `GetPlayerWantedLevel`)

Level wanted per pemain ala SA-MP: 0 = bersih (tak dicari polisi) ..
6 = paling dicari. State otoritatif server (`Client.wanted_level`,
default 0 termasuk NPC). Setiap `romp.setPlayerWantedLevel` yang berhasil
di-broadcast ke SEMUA pemain via paket `0x48` WANTED_SET (global —
wanted terlihat semua orang ala SA-MP, tanpa scope vw/interior) +
menyalakan event Lua `onPlayerWantedLevelChange(pid, oldlevel, newlevel)`
(perpanjangan RO-MP; SA-MP tidak punya event ini). Pemain yang baru join
di-resync wanted semua yang connect (termasuk miliknya sendiri).

Event ini memudahkan logika polisi di gamemode: kirim mobil patroli
(cop car) saat wanted >= 3, ganti skin polisi, dsb. Demo di
`server/gamemode.lua`: `/wanted <0-6>` (set wanted sendiri), `/wantedof
<id>` (lihat wanted pemain lain) + handler `onPlayerWantedLevelChange`
yang mengumumkan pengerahan polisi saat level >= 3.

| API | Keterangan |
|-----|------------|
| `romp.setPlayerWantedLevel(pid, level)` | Set wanted 0..6 → `true`/`false`. Broadcast `0x48` + event. |
| `romp.getPlayerWantedLevel(pid)` | Level wanted saat ini (int) / nil bila pemain tak valid. |

Event: `onPlayerWantedLevelChange(pid, oldlevel, newlevel)` — menyala
setiap perubahan wanted yang berhasil (level 0 = bersih).

```lua
-- /wanted: set wanted sendiri ke 4 (polisi siaga).
romp.setPlayerWantedLevel(id, 4)

-- Event: kerahkan mobil patroli saat wanted tinggi.
function onPlayerWantedLevelChange(pid, oldlevel, newlevel)
    if newlevel >= 3 then
        local n = romp.getPlayerName(pid) or ("id " .. pid)
        romp.sendChatToAll(romp.KIND_SYS,
            "*** 🚔 " .. n .. " wanted level " .. newlevel
            .. " — polisi mengerahkan mobil patroli!")
    end
end
```

Paket: `0x48` WANTED_SET (frame 3 byte: u8 pid + u8 level, server →
semua client, global) — lihat `PROTOCOL.md`. Client C++:
`IROMPPlatform::OnPlayerWanted`; Godot: sinyal `wanted_set`.

### Uang pemain otoritatif ala SA-MP (`SetPlayerMoney` / `GivePlayerMoney` / `ResetPlayerMoney` / `GetPlayerMoney`)

Saldo per pemain ala SA-MP. State otoritatif server (`Client.money`,
default 0 termasuk NPC). Setiap `romp.setPlayerMoney` /
`romp.givePlayerMoney` / `romp.resetPlayerMoney` yang berhasil mengirim
paket `0x49` MONEY_SET HANYA ke pemain bersangkutan (per-pemain — uang
hanya tampil di HUD milik sendiri ala SA-MP, orang lain tak melihat) +
menyalakan event Lua `onPlayerMoneyChange(pid, oldamount, newamount)`
(perpanjangan RO-MP; SA-MP tak punya event ini). Pemain yang baru join
di-resync saldo miliknya sendiri.

Anti-dupe dasar: server satu-satunya sumber kebenaran — client tidak
pernah bisa mengubah uangnya sendiri (tak ada paket client -> server
untuk uang). Semua perubahan lewat API server; rentang di-clamp ke
-999999999..999999999 sehingga overflow tak mungkin terjadi
(`givePlayerMoney` menjenuhkan hasil, bukan melimpah).

| API | Keterangan |
|-----|------------|
| `romp.setPlayerMoney(pid, amount)` | Set saldo (rentang -999999999..999999999) → `true`/`false`. Kirim `0x49` + event. |
| `romp.givePlayerMoney(pid, delta)` | Tambah/kurangi saldo (negatif = kurangi) → `true`/`false`. Hasil dijenuhkan ke rentang. |
| `romp.resetPlayerMoney(pid)` | Saldo kembali ke 0 → `true`/`false`. |
| `romp.getPlayerMoney(pid)` | Saldo saat ini (int) / nil bila pemain tak valid. |

Event: `onPlayerMoneyChange(pid, oldamount, newamount)` — menyala
setiap perubahan saldo yang berhasil.

```lua
-- /kasih: transfer dari saldo sendiri ke pemain lain (anti-dupe:
-- transfer hanya lewat server, saldo diverifikasi dulu).
local mine = romp.getPlayerMoney(id) or 0
if mine >= amt then
    romp.givePlayerMoney(id, -amt)
    romp.givePlayerMoney(tp, amt)
end

-- Event: umumkan transaksi besar ala berita ekonomi RP.
function onPlayerMoneyChange(pid, oldamount, newamount)
    if math.abs(newamount - oldamount) >= 1000000 then
        romp.sendChatToAll(romp.KIND_SYS,
            "*** Transaksi besar: " .. oldamount .. " -> " .. newamount)
    end
end
```

Paket: `0x49` MONEY_SET (frame 6 byte: u8 pid + s32 amount LE, server →
pemilik saja) — lihat `PROTOCOL.md`. Client C++:
`IROMPPlatform::OnPlayerMoney`; Godot: sinyal `money_set`.
Demo di `server/gamemode.lua`: `/uang` (cek saldo), `/setuang <id>
<jumlah>` (set), `/kasih <id> <jumlah>` (transfer).

### World settings ala SA-MP (gravity, weather, world time)

State dunia bersifat global (sama untuk semua pemain) dan otoritatif di
server. Nilai awal dari `server.cfg` (`gravity`, `weather`, `worldtime`),
bisa diubah runtime dari Lua — setiap perubahan di-broadcast ke semua client
dan dikirim ulang ke pemain yang baru join.

| API | Keterangan |
|-----|------------|
| `romp.setGravity(gravity)` | → `true`/`false`; valid 0.001..0.5 (default GTA:SA 0.008) |
| `romp.getGravity()` | Gravitasi saat ini (float) |
| `romp.setWeather(weatherid)` | → `true`/`false`; valid 0..255 (default 10 = Sunny) |
| `romp.getWeather()` | Cuaca saat ini (int) |
| `romp.setWorldTime(hour [, minute])` | → `true`/`false`; hour 0..23, minute 0..59 |
| `romp.getWorldTime()` | → `hour, minute` |

Paket: `0x39` WORLD_SET (lihat `PROTOCOL.md`). Tak ada event
(world settings bersifat pasif, seperti di SA-MP).

```lua
-- Contoh: zona moon gravity + cuaca badai + tengah malam.
function onPlayerCommand(id, name, cmd, args)
    if cmd == "bulan" then
        romp.setGravity(0.002)
        romp.setWeather(17)          -- hujan
        romp.setWorldTime(0, 0)      -- tengah malam
        return true
    end
    return false
end
```

### Vehicle params ala SA-MP (SetVehicleParamsEx / SetVehicleParamsForPlayer)

State params kendaraan otoritatif di server. Params global (engine, lights,
alarm, doors, bonnet, boot, objective) di-broadcast ke virtual world
kendaraan tiap berubah dan di-resync ke pemain yang baru join / pindah
world. Params per-pemain (objective, doorslocked) hanya dikirim ke pemain
tersebut. Nilai tiap param: `-1` = jangan ubah / unset, `0` = mati,
`1` = nyala.

| API | Keterangan |
|-----|------------|
| `romp.setVehicleParamsEx(vid, engine, lights, alarm, doors, bonnet, boot, objective)` | → `true`/`false`; nilai valid -1/0/1 |
| `romp.getVehicleParamsEx(vid)` | `{engine,lights,alarm,doors,bonnet,boot,objective}` / nil |
| `romp.setVehicleParamsForPlayer(vid, pid, objective, doorslocked)` | → `true`/`false`; hanya ke pemain tsb |
| `romp.getVehicleParamsForPlayer(vid, pid)` | `{objective,doorslocked}` / nil (tanpa entri = -1/-1) |

Paket: `0x3D` VEH_PARAMS / `0x3E` VEH_PARAMS_P (lihat `PROTOCOL.md`).
Tak ada event (params bersifat pasif, seperti di SA-MP).

```lua
-- Contoh: matikan mesin + kunci pintu Infernus demo.
romp.setVehicleParamsEx(vid, 0, -1, -1, 1, -1, -1, -1)  -- engine mati, doors kunci
romp.setVehicleParamsForPlayer(vid, pid, 1, -1)         -- marker objective utk 1 pemain
```

### Plat nomor kendaraan ala SA-MP (`SetVehicleNumberPlate`)

Plat nomor teks per kendaraan, otoritatif di server. Setiap
`romp.setVehicleNumberPlate(vid, plate)` yang berhasil di-broadcast ke
virtual world kendaraan via paket `0x4A` (dan di-resync ke pemain yang
baru join / pindah vw bersama modifikasi lain). String kosong menghapus
plat. Demo: `/plat <teks>` (tanpa argumen = hapus) pada kendaraan demo
`/modif` di `server/gamemode.lua`.

| API | Keterangan |
|-----|------------|
| `romp.setVehicleNumberPlate(vid, plate)` | Teks plat maks 32 char (dipotong bila lebih panjang), `""` = hapus → `true`/`false` |
| `romp.getVehicleNumberPlate(vid)` | Teks plat saat ini / nil bila kendaraan tak valid |

`romp.getVehicles()` kini membawa field `plate` per kendaraan.

### Spectate ala SA-MP (`TogglePlayerSpectating` / `PlayerSpectatePlayer` / `PlayerSpectateVehicle`)

Mode penonton, otoritatif di server. Setiap perubahan status di-broadcast
ke virtual world si penonton via paket `0x4B` (dan di-resync ke pemain yang
baru join / pindah vw): client si penonton mengalihkan kamera mengikuti
target (mode 0=normal, 1=fixed, 2=side), client lain menyembunyikan ped si
penonton (tetap ada di daftar pemain & scoreboard). Selama spectate, paket
posisi `0x10` dari si penonton dibuang server (posisi beku ala SA-MP).
Auto-stop saat target disconnect / kendaraan di-destroy / pindah vw
melewati batas world. NPC tak bisa jadi penonton (tak punya kamera) tapi
boleh jadi target. Demo: `/spec <id> [mode]`, `/specv <vid> [mode]`,
`/unspec` di `server/gamemode.lua`.

| API | Keterangan |
|-----|------------|
| `romp.togglePlayerSpectating(pid, on)` | Masuk/keluar mode penonton → `true`/`false` (`false` bila pemain tak valid / NPC) |
| `romp.playerSpectatePlayer(pid, targetpid[, mode])` | Kamera mengikuti pemain (otomatis masuk mode spectate); mode default 0 → `true`/`false` |
| `romp.playerSpectateVehicle(pid, vid[, mode])` | Kamera mengikuti kendaraan → `true`/`false` |
| `romp.isPlayerSpectating(pid)` | `true` bila pemain sedang spectate |
| `romp.getPlayerSpectateTarget(pid)` | `{type="player"\|"vehicle"\|"none", id, mode}` / nil bila tidak spectate |

Event `onPlayerSpectate(pid, spectating, targettype, target, mode)` —
perpanjangan RO-MP (SA-MP tak punya event ini); `targettype`
0=none/1=pemain/2=kendaraan. Konstanta:
`romp.SPECTATE_TARGET_NONE/PLAYER/VEHICLE`,
`romp.SPECTATE_MODE_NORMAL/FIXED/SIDE`.

### Database SQLite untuk gamemode (skor, akun)

Tanpa server eksternal: cukup set `database <path>` di `server.cfg`
(mis. `database romp.db`), file dibuat otomatis. Database dibuka sebelum
`onGamemodeInit` dan tersedia sebagai **`romp.db`** di semua script
(gamemode + filterscript). SQLite 3.54 ikut di-bundle di dalam binari.

```lua
function onGamemodeInit()
    romp.dbExec(romp.db, [[CREATE TABLE IF NOT EXISTS akun (
        nama TEXT PRIMARY KEY, skor INTEGER, uang REAL)]])
end

function onPlayerJoin(id, name)
    -- INSERT aman: pakai "?" + parameter, BUKAN string.format (anti SQL injection)
    romp.dbExec(romp.db,
        "INSERT OR IGNORE INTO akun (nama, skor, uang) VALUES (?, 0, 0)", name)
    local rows = romp.dbQuery(romp.db,
        "SELECT skor, uang FROM akun WHERE nama = ?", name)
    if rows and rows[1] then
        romp.sendChatToPlayer(id, romp.KIND_INFO,
            string.format("Selamat datang kembali! Skor: %d", rows[1].skor))
    end
end
```

| Fungsi | Keterangan |
|---|---|
| `romp.dbOpen(path)` | Buka DB tambahan → `dbid` / `nil, errmsg` |
| `romp.dbClose(db)` | Tutup DB (jangan tutup `romp.db`) → `true` |
| `romp.dbExec(db, sql, ...)` | INSERT/UPDATE/DELETE/CREATE → jumlah baris berubah / `nil, errmsg` |
| `romp.dbQuery(db, sql, ...)` | SELECT → `{ {kolom=nilai, ...}, ... }` / `nil, errmsg` |
| `romp.dbError(db)` | Pesan error terakhir (string) |
| `romp.db` | Handle default dari `server.cfg`, `nil` bila `database` kosong |
| `romp.getConfig("database")` | Path database dari config |

Tipe kolom: INTEGER→integer, REAL→number, TEXT/BLOB→string, NULL→`nil`
(key hilang dari tabel baris). Boolean Lua → 0/1. `dbQuery` hanya menyiapkan
satu statement per panggilan.

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
| `vehicles` | Lihat kendaraan aktif |
| `kick <id> [alasan]` | Kick pemain |
| `ban <ip>` / `unban <ip>` / `bans` | Kelola ban IP |
| `loadfs <nama>` | Load filterscript (`filterscripts/<nama>.lua`) |
| `unloadfs <nama>` | Unload filterscript |
| `reloadfs <nama>` | Reload filterscript |
| `filterscripts` | Daftar filterscript yang aktif |
| `stop` | Matikan server |

Command console juga tersedia lewat RCON (UDP, butuh `rcon_password`).
Lihat `PROTOCOL.md` bagian "Filterscript ala SA-MP" untuk detail modul Lua
terpisah yang bisa di-load/unload tanpa restart server.

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
- Kendaraan: sync posisi, penumpang, health, komponen tuning (spoiler/velg/dsb,
  paket `0x3A`-`0x3C`) & paintjob & plat nomor (`0x4A` VEH_PLATE) ala SA-MP
  (demo `/modif` + `/plat`)

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
