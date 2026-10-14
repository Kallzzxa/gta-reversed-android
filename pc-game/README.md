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

- [ ] Klien multiplayer: sambungkan ke protokol RO-MP (`server/`)
      — kirim posisi 10Hz, chat, sinkron pemain lain & kendaraan.
- [ ] NPC pejalan kaki & lalu lintas sederhana.
- [ ] Interior/garasi, sistem wanted ala SA-MP.
- [ ] Export Windows (.exe) siap main.
