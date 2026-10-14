# RO-MP ROADMAP

Daftar fitur ala SA-MP yang dikerjakan bertahap (satu per sesi update).
Setiap fitur: implementasi → compile/test → commit (English) → push.

## Server (dedicated)

- [x] Sistem dialog ala SA-MP (DIALOG_STYLE_MSGBOX/LIST/INPUT/PASSWORD) via Lua + NUI — DONE 2026-10-14 (romp.showDialog/hideDialog, event onDialogResponse, dialog.html)
- [x] Textdraw global & per-pemain (teks 2D di layar) via Lua API — DONE 2026-10-14 (server/textdraw.lua auto-load: create/show/hide/setString/setStyle/destroy ala TextDraw* & PlayerTextDraw*, resync saat join; textdraw.html; demo /td /tdstr /tdhide /ptd; test 21/21)
- [x] Checkpoint & race checkpoint (masuk/keluar event Lua) — DONE 2026-10-14 (server/checkpoint.lua auto-load: create/destroy/show/hide/getPlayerCheckpoint/setCheckpointPos/setRaceCheckpointNext/getCheckpointPos/isPlayerInCheckpoint ala SetPlayerCheckpoint/SetPlayerRaceCheckpoint + event onPlayerEnter/LeaveCheckpoint & onPlayerEnter/LeaveRaceCheckpoint via onTick 1 dtk; posisi pemain di-track server-side (paket 0x10) + romp.getPlayerPos; NUI checkpoint.html; test 16/16)
- [x] Pickup (senjata, health, custom) dengan event onPlayerPickup — DONE 2026-10-14 (server/pickup.lua auto-load: create/destroy/show/hide/isVisible/getPos/setPos/getModel/setRespawnTime ala AddStaticPickup + tipe RESPAWN/ONCE/ALWAYS + event onPlayerPickup via onTick 1 dtk radius 1.5 memakai romp.getPlayerPos; state per-pemain; NUI pickup.html; PROTOCOL.md dokumentasi; test 20/20)
- [x] Map icon / radar blip per pemain via Lua — DONE 2026-10-14 (server/mapicon.lua auto-load: setPlayerMapIcon/removePlayerMapIcon/removeAllPlayerMapIcons/getPlayerMapIcon ala SetPlayerMapIcon/RemovePlayerMapIcon, iconid 0-99, markertype local/global/local-checkpoint/global-checkpoint, warna 0xRRGGBBAA, style permanen/berkedip, playerPos tiap detik via onTick; NUI mapicon.html radar melingkar + blip; PROTOCOL.md dokumentasi; binari linux+windows rebuild; test 24/24)
- [x] Sinkronisasi kendaraan: spawn, posisi, penumpang, damage — DONE 2026-10-14 (server C otoritatif: server/src/romp_vehicle.c — paket 0x12 VEH (sopir→server, hanya sopir boleh sync) → relay 0x13 VEH_SYNC, 0x14 VEH_SPAWN / 0x15 VEH_DESPAWN / 0x16 VEH_SEAT, stream kendaraan ke pemain baru saat join, cleanup saat disconnect, respawn otomatis; API Lua romp.createVehicle/destroyVehicle/setVehiclePos/getVehiclePos/setVehicleHealth/getVehicleHealth/damageVehicle/putPlayerInVehicle/removePlayerFromVehicle/isPlayerInVehicle/getPlayerVehicle(Seat)/getVehicleModel/getVehicleColors/setVehicleColors/getVehicleDriver/getVehiclePassenger/respawnVehicle/getVehicles; event onPlayerEnterVehicle/onPlayerExitVehicle/onVehicleDamage/onVehicleDeath/onVehicleSpawn; health 0-1000 ala SA-MP, <=250 = hancur; server/vehicle.lua auto-load untuk NUI vehicle.html (daftar + health bar, di-diff tiap detik); PROTOCOL.md dokumentasi; binari linux+windows rebuild; test 39/39)
- [x] Sinkronisasi senjata & tembakan (combat dasar) — DONE 2026-10-14 (server C otoritatif untuk inventory: server/src/romp_weapon.c — paket 0x17 WPN_SHOT (weaponid/hittype/hitid/titik kena) → validasi: senjata harus dimiliki (romp.giveWeapon), weaponid valid 1..46, rate limit 25/detik, range ≤400 unit dari posisi, target pemain/kendaraan valid → relay 0x18 WPN_SYNC (tak di-echo ke penembak) + 0x19 WPN_SET (set slot inventory ke pemain); API Lua romp.giveWeapon/removePlayerWeapon/resetPlayerWeapons/getPlayerWeapon/getPlayerWeaponData/weaponSlot + konstanta WPN_HIT_NONE/PLAYER/VEHICLE/OBJECT + slot mapping ala SA-MP 0-12; event onPlayerWeaponShot(pid, weaponid, hittype, hitid, x, y, z) ala OnPlayerWeaponShot; damage tidak dihitung server (gamemode yang memutuskan); PROTOCOL.md dokumentasi; binari linux+windows rebuild; test 19/19)
- [x] Sistem interior & virtual world per pemain — DONE 2026-10-14 (server C: interior 0-255 & vw 0-2^31-1 per pemain, default 0/0; visibility = vw & interior sama ala SA-MP → scope 0x11 PPOS, chat pemain & /me, 0x18 WPN_SYNC, 0x22/0x23 entity, 0x24 PLAYER_LIST; paket baru 0x25 INT_VW ke pemain saat berubah; resync dua arah P_JOIN/P_LEAVE + stream kendaraan saat pindah; pengumuman server tetap global; kendaraan punya vw sendiri: 0x14/0x15/0x16 vw-scoped, 0x13 scope ke viewer sopir, kendaraan ikut pindah vw bila sopir pindah; API Lua romp.getPlayerInterior/setPlayerInterior/getPlayerVirtualWorld/setPlayerVirtualWorld/getVehicleVirtualWorld/setVehicleVirtualWorld + getPlayers bawa interior & vw; event onPlayerInteriorChange/onPlayerVirtualWorldChange; PROTOCOL.md dokumentasi; binari linux+windows rebuild; test 40/40)
- [x] RCON console (remote admin via password) — DONE 2026-10-14 (UDP di port game: 0x05 RCON / 0x06 RCON_REPLY; config `rcon` + `rcon_password` di server.cfg, tanpa password RCON mati total; command sama dengan console lokal — say/kick/players/vehicles/stop + baru ban/unban/bans; output command dikembalikan ke client; anti brute-force 2/detik per IP, 5x salah = blokir 60 dtk; console lokal di-refactor ke console_run() agar output bisa ditangkap RCON; tool `server/rcon.py`; PROTOCOL.md dokumentasi; binari linux+windows rebuild; console path terverifikasi manual, test UDP penuh di tests/test_rcon.py — sandbox blokir UDP jadi end-to-end belum bisa jalan di VM)
- [ ] Filterscript ala SA-MP (modul Lua terpisah yang bisa di-load/unload)
- [ ] NPC/bot (koneksi virtual untuk testing & populasi)
- [ ] Anti-cheat dasar: deteksi teleport, speed hack, health hack
- [ ] MySQL/SQLite opsional untuk gamemode (skor, akun)

## Protokol & Browser

- [ ] Server browser in-game (Android): tab Internet/LAN/Favorit — DONE (v1)
- [x] Favorit ala SA-MP Mobile Launcher (ROMPFavorites.java + JSON, tambah/hapus, tab Favorit) — DONE 2026-10-14
- [ ] Password server di browser (prompt sebelum connect)
- [ ] Sorting & filter browser (nama, pemain, ping)

## Client Android

- [ ] JNI penuh: chat, NUI WebView, browser (saat ini contoh saja)
- [ ] Render pemain remote (ped) di game
- [ ] Nametag + chat bubble 3D
- [ ] Scoreboard (TAB): daftar pemain + ping

## Client PC (RO-MP-PC)

- [x] Game 3D standalone `pc-game/` (Godot 4, dibuat sendiri, fase 1 single-player) — DONE 2026-10-14: kota prosedural 5x5 blok (jalan, gedung, lampu, pohon), karakter third-person (WASD/Shift/Space), mobil arcade dikendarai (E/F), kamera orbit + SpringArm, HUD, langit & bayangan. Cara main: `godot --path pc-game`.
- [ ] Klien multiplayer Godot: sambungkan `pc-game/` ke protokol RO-MP (posisi 10Hz, chat, pemain & kendaraan remote).
- [ ] romp.exe: PLAY stabil, deteksi GTA otomatis — DONE (v1.1.0)
- [ ] romp.dll: build & test in-game pertama kali
- [ ] romp-setup.exe: build & test installer NSIS
- [ ] Sinkronisasi kendaraan & penumpang di mod
- [ ] Scoreboard & dialog SA-MP di dalam game

## Lua Gamemode API (ala FiveM, rasa SA-MP)

- [x] Event dasar: join/leave/chat/command/tick — DONE
- [x] Event: onGamemodeInit, onConsoleCommand, onPlayerRequestConnect — DONE
- [ ] API pemain: setPos, getPos, setHealth, setArmour, giveWeapon
- [ ] API kendaraan: createVehicle, destroyVehicle, setVehiclePos
- [x] API textdraw & dialog — DONE 2026-10-14 (dialog: commit 5615807; textdraw: server/textdraw.lua + textdraw.html)
- [ ] Timer Lua: setTimer / killTimer
