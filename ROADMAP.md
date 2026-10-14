# RO-MP ROADMAP

Daftar fitur ala SA-MP yang dikerjakan bertahap (satu per sesi update).
Setiap fitur: implementasi → compile/test → commit (English) → push.

## Server (dedicated)

- [x] Sistem dialog ala SA-MP (DIALOG_STYLE_MSGBOX/LIST/INPUT/PASSWORD) via Lua + NUI — DONE 2026-10-14 (romp.showDialog/hideDialog, event onDialogResponse, dialog.html)
- [ ] Textdraw global & per-pemain (teks 2D di layar) via Lua API
- [ ] Checkpoint & race checkpoint (masuk/keluar event Lua)
- [ ] Pickup (senjata, health, custom) dengan event onPlayerPickup
- [ ] Map icon / radar blip per pemain via Lua
- [ ] Sinkronisasi kendaraan: spawn, posisi, penumpang, damage
- [ ] Sinkronisasi senjata & tembakan (combat dasar)
- [ ] Sistem interior & virtual world per pemain
- [ ] RCON console (remote admin via password)
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
- [ ] API textdraw & dialog
- [ ] Timer Lua: setTimer / killTimer
