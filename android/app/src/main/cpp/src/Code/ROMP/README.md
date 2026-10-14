# Modul Client RO-MP

Client multiplayer RO-MP untuk gta-reversed-android. 100% portable:
hanya pakai **POSIX socket + C++20 standar**, tanpa API khusus Android —
modul ikut ke-build otomatis oleh `CMakeLists.txt` (GLOB_RECURSE `*.cpp`)
dan bisa di-compile + di-test standalone di Linux.

Protokol yang dipakai: `server/PROTOCOL.md` (TCP, frame
`[u16 len LE][u8 type][payload]`).

## File

| File | Isi |
|---|---|
| `ROMP_Protocol.h` | Konstanta protokol (mirror persis PROTOCOL.md) + helper build/parse frame. Header-only. |
| `ROMP_Net.h` / `.cpp` | `ROMPNet`: TCP client non-blocking (connect + select-based Pump, reassembly frame, callback per paket). |
| `ROMP_Chat.h` / `.cpp` | `ROMPChat`: ring buffer chat (tampil 12 baris, histori 100) + input buffer ketikan user. |
| `ROMP_Players.h` / `.cpp` | `ROMPPlayers`: tabel pemain remote + interpolasi linear (lerp eksponensial, update 10 Hz dari server). |
| `ROMP.h` / `.cpp` | `ROMP::Client` (singleton, facade) + `IROMPPlatform` + `NullPlatform`. |

Contoh Java (tidak ikut build native, tidak di-reference file lain):

- `android/app/src/main/java/com/regta/romp/ROMPChatInput.java` — Dialog
  input chat ala SA-MP mobile (EditText + tombol Kirim). Method native
  `sendChatNative(String)` dideklarasikan, tapi **implementasi JNI-nya di
  reGTA.cpp masih TODO** (lihat komentar di file).

## Cara integrasi ke game

1. **Tiap frame** (mis. hook di `CGame::Process`), panggil:
   ```cpp
   #include "Code/ROMP/ROMP.h"
   ROMP::Client::Instance().Update();
   ```
   `Update()` menangani: Pump jaringan, kirim POS tiap 100 ms,
   interpolasi + `MoveRemotePlayer()` tiap frame, PING tiap 15 detik.

2. **Sambungkan platform game** sekali saat start (ganti NullPlatform):
   ```cpp
   class GamePlatform : public ROMP::IROMPPlatform { /* ... */ };
   static GamePlatform g_platform;
   ROMP::Client::Instance().Init(&g_platform);
   ```

3. **Connect** (mis. dari menu / tombol "Multiplayer"):
   ```cpp
   if (ROMP::Client::Instance().Connect("192.168.1.10", 7777, "Andi", ""))
       ; // WELCOME diterima, resmi join
   ```

4. **Chat UI game**: baca baris tampil via
   `Client::Instance().Chat().GetVisibleLines(...)` lalu gambar pakai
   `CFont` (TODO). Warna per `ChatKind`: Normal=putih, Server=emas,
   Me=ungu, PM=kuning, JoinLeave=abu. Input chat: pakai
   `ROMPChatInput` (Java) atau `Chat().SetInput()/TakeInput()` lalu
   `Client::Instance().SendChat(teks)`.

## Alur otomatis yang sudah ditangani

- `WELCOME`/`REJECT` saat handshake `Connect()` (timeout 5 detik).
- `PLAYER_JOIN`/`PLAYER_LEAVE` → update tabel + `SpawnRemotePlayer()` /
  `RemoveRemotePlayer()` + baris chat abu-abu.
- `PLAYER_LIST` (setelah WELCOME) → spawn semua pemain online.
- `PLAYER_POS` → update target interpolasi; `Update()` me-lerp lalu
  memanggil `MoveRemotePlayer(id, x, y, z, heading)` tiap frame.
- `CHAT_MSG` → masuk buffer chat (teks sudah diformat server).
- `PING`/`PONG` otomatis tiap 15 detik.
- Disconnect (EOF/error) → `Disconnect()` bersih: hapus semua remote
  player dari game + baris chat sistem.

## Test

```bash
cd ~/workspace/romp/tests/client
./run_test.sh
```

Menjalankan server asli di port 17777 lalu menghajar skenario penuh:
connect+HELLO→WELCOME, echo CHAT_MSG, PLAYER_JOIN/POS/CHAT dari client
fake kedua (socket mentah), PLAYER_LEAVE, disconnect bersih. 13 cek —
semua harus PASS.

## TODO integrasi game (cari `TODO(GAME)` di kode)

- `GetLocalPlayerPos()` — baca dari `CPed` pemain lokal (`FindPlayerPed()`).
- `SpawnRemotePlayer()` — buat `CPed`/actor via `CWorld::Add`, model CJ.
- `RemoveRemotePlayer()` — hapus `CPed` yang dibuat di atas.
- `MoveRemotePlayer()` — set posisi `CPed` (hasil interpolasi).
- Render chatbox: `CFont::PrintString` per baris dari `GetVisibleLines()`.
- Tombol chat HUD → tampilkan `ROMPChatInput.show(context)`.
- JNI `Java_com_regta_romp_ROMPChatInput_sendChatNative` di `reGTA.cpp`
  → panggil `ROMP::Client::Instance().SendChat(...)`.
- (Opsional) hitung RTT dari `PONG` untuk indikator ping.
