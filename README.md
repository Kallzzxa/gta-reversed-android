## GTA SA Reversed Android

A project to reverse Grand Theft Auto San Andreas (2.10 Android) completely, and to rewrite and document every function.

### Coding Guidelines 
Before you start writing code, please make sure to read the [coding guidelines](docs/CodingGuidelines.MD) for this project.

### How to Set Up and Build in Android Studio 

Wanna get started? It’s simple! Follow these steps:  

1. **Download Android Studio**  
   If you don’t have Android Studio yet, grab it here: [Android Studio](https://developer.android.com/studio).  

2. **Clone the repository**  
   Fire up your terminal and run:  
   ```bash
   git clone https://github.com/gta-android/gta-android.git
   cd gta-android
   ```  

3. **Open it in Android Studio**  
   - Launch Android Studio.  
   - Click **"Open"** and select the folder where you just cloned the project and select the **"Android"** folder.

4. **Install dependencies**  
   Android Studio should automatically detect and set up the dependencies. If not, go to **File > Sync Project with Gradle Files**.  

5. **Set up a test device**  
   - Use an Android emulator from Android Studio or connect a real Android device.  
   - Make sure **USB Debugging** is enabled on your device.  

6. **Build and run**  
   - Click the **Run** button (the ▶️ icon) in the top bar.  
   - Choose your test device, sit back, and let it do its thing!  

7. **Final tips**  
   If you run into issues, hop onto our Discord! The community’s always ready to help.

### Developing Tools

Minimum:
* **Windows 10/11 x64** (Recommended LTCS version)
* **[Android Studio](https://developer.android.com/studio)**

Recomended: 
* **GTA: SA 2.10 v34 APK**
* **IDA Pro 9.0+**
* **[7-Zip](https://www.7-zip.org/download.html)** 
* **[HxD](https://mh-nexus.de/en/downloads.php)** (Hex Editor)
* **[VS Code](https://code.visualstudio.com/download)**
* **[Everything](https://www.voidtools.com/downloads/)** (Search engine)
* **[Github Desktop](https://desktop.github.com/download/)** (Disable Analytics in Options)

### RO-MP — Multiplayer ala SA-MP 🎮

Project gabut: bikin SA-MP versi sendiri di atas game ini, namanya **RO-MP**.

- **Server dedicated** di [`server/`](server/) — satu binari kecil tanpa dependency,
  tinggal jalanin. Tersedia untuk **Windows** (`bin/windows/romp-server.exe`) dan
  **Linux** (`bin/linux/romp-server`), plus `server.cfg`, script start, dan
  dokumentasi protokol ([`server/PROTOCOL.md`](server/PROTOCOL.md)).
  Cara jalanin: `./start-server.sh` (Linux) atau klik `start-server.bat` (Windows).
- **Modul client** di `android/app/src/main/cpp/src/Code/ROMP/` — network client
  (TCP), chat buffer, sync posisi pemain remote + interpolasi. 100% portable
  (POSIX + C++20), otomatis ke-build oleh CMake. Titik integrasi ke fungsi game
  asli ditandai `TODO(GAME)` — modul sudah dites end-to-end melawan server asli.
- Contoh dialog input chat Android: `android/app/src/main/java/com/regta/romp/ROMPChatInput.java`.

Fitur v1: join/leave, chat global + `/me` `/pm` `/players` `/help` `/q`,
broadcast posisi 10 Hz, password server, anti-spam, console admin
(`say`/`kick`/`players`/`stop`).

### Community 

[![Discord](https://dcbadge.limes.pink/api/server/Nr4MfSEGXd)](https://discord.gg/Nr4MfSEGXd)

### Credits
- All contributors to the original PC version project [gta-reversed-pc](https://github.com/gta-reversed/gta-reversed).
- And everyone who contributed to GTA:SA reversing.
