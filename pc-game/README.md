# RO-MP PC

Game 3D standalone ala GTA untuk RO-MP, dibuat sendiri dengan **Godot 4**.
Fase 1: single-player sandbox — kota prosedural, karakter third-person,
mobil yang bisa dikendarai.

## Cara main (PC)

Jalankan dengan Godot 4.3+:

```bash
godot --path pc-game
```

Atau export ke Windows/Linux via editor: Project → Export.

### Kontrol

| Tombol | Aksi |
|---|---|
| WASD | Jalan |
| Shift | Lari |
| Space | Lompat / handbrake (di mobil) |
| Mouse | Putar kamera |
| E | Masuk mobil (dekat mobil) |
| F | Keluar mobil |
| ESC | Lepas mouse |

### Multiplayer (fase 2)

Game sekarang bisa connect ke server RO-MP sungguhan via TCP
(`server/PROTOCOL.md`):

- **Menu koneksi** saat game dibuka: nickname, IP, port, password
  (atau "Main Offline" / ESC untuk sandbox).
- **Posisi 10Hz** (`0x10 POS`) + jawab probe ping server (`0x30`/`0x31`).
- **Pemain remote**: ped kapsul + nametag 3D (`Label3D`), interpolasi posisi,
  snap saat teleport jauh; NPC ditandai `[BOT]`.
- **Chat ala SA-MP**: `T` buka input, Enter kirim, ESC batal — warna per kind
  (putih/emas/ungu `/me`/kuning PM/abu join-leave).
- **Kendaraan remote**: `0x14`/`0x13`/`0x15` (spawn/sync/despawn),
  interpolasi, indikator 🔥 bila hancur (health ≤ 250).
- **Scoreboard**: tahan `TAB` — ID, nama, ping sendiri.
- **Teleport admin**: `0x27 POS_SET` (mis. via `romp.setPlayerPos`).

File baru: `scripts/romp_net.gd` (klien protokol: frame, handshake
HELLO/WELCOME/REJECT, semua paket server→client, kirim POS/CHAT/VEH),
`scripts/remote_player.gd`, `scripts/remote_car.gd`, `scripts/mp_menu.gd`,
`scripts/mp_chat.gd`, `scripts/mp_scoreboard.gd`;
`scripts/main.gd` + `scripts/hud.gd` diintegrasikan.

Batasan v1: mobil lokal tetap properti client-side (tidak tersync ke
server); sopir kendaraan server (`0x12`) & dialog NUI (`0x40`) belum;
chat/command server lain tetap jalan normal.

Test: `~/workspace/romp/tests/test_godot_mp.py` — Godot headless
(`pc-game/tests/mp_client_test.gd`) bicara dengan server asli:
handshake+password, chat dua arah, PLAYER_POS, VEH_SPAWN, POS_SET,
PING/PONG, REJECT password salah — 9/9 lolos.

## Isi fase 1

- `scripts/city_generator.gd` — kota prosedural: grid 5x5 blok, jalan +
  marka, trotoar, gedung acak (collision), lampu jalan, pohon.
- `scripts/player.gd` — karakter third-person: jalan/lari/lompat,
  gerak relatif kamera, masuk mobil.
- `scripts/car.gd` — mobil arcade: gas/rem/mundur, setir sensitif
  kecepatan, handbrake, roda berputar.
- `scripts/camera_rig.gd` — kamera orbit + SpringArm anti-tembus gedung.
- `scripts/hud.gd` — HUD: kecepatan & petunjuk kontrol.
- Langit prosedural + matahari + bayangan.

## Roadmap

- [x] Klien multiplayer: sambungkan ke protokol RO-MP (`server/`)
      — kirim posisi 10Hz, chat, sinkron pemain lain & kendaraan.
      — DONE 2026-10-15: `scripts/romp_net.gd` (klien TCP: handshake
      HELLO/WELCOME/REJECT, parse semua paket server→client, kirim
      POS/CHAT/VEH, jawab probe ping 0x30, ukur RTT), `remote_player.gd`
      (ped + nametag 3D + interpolasi + snap teleport + tag [BOT]),
      `remote_car.gd` (spawn/sync/despawn + interpolasi + indikator hancur),
      `mp_menu.gd` (nickname/IP/port/password), `mp_chat.gd` (T/Enter/ESC,
      warna per kind), `mp_scoreboard.gd` (TAB: id/nama/ping), integrasi
      `main.gd`/`hud.gd` (status ●/○ + POS_SET teleport); test
      `tests/test_godot_mp.py` + `pc-game/tests/mp_client_test.gd` —
      9/9 lolos vs server asli (Godot 4.3 headless)
- [ ] NPC pejalan kaki & lalu lintas sederhana.
- [ ] Interior/garasi, sistem wanted ala SA-MP.
- [ ] Export Windows (.exe) siap main.
