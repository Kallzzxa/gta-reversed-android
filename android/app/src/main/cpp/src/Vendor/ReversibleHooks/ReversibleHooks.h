//
// ReversibleHooks.h — STUB KOMPATIBILITAS LOKAL
//
// Header asli ReversibleHooks.h dirahasiakan oleh upstream
// (lihat .gitignore: "# Private ReversibleHooks"), sehingga repo ini
// tidak bisa di-build tanpa file ini.
//
// File ini adalah stub clean-room: API-nya meniru yang dipakai oleh
// reGTA.cpp / Hooks.cpp / InjectReversedClasses.cpp.
//   - FindLib() diimplementasikan beneran (pindai /proc/self/maps,
//     pendekatan standar di Android).
//   - Redirect() & InitHookStuff() masih no-op karena injector aslinya
//     belum dirilis upstream ("Our injector will soon be available").
//     Artinya: hook belum terpasang saat runtime sampai injector asli ada.
//
// Kalau upstream merilis header aslinya, GANTI file ini dengan yang asli.
//
// Dibuat untuk membuat `gradlew assembleDebug` jalan out-of-the-box.
//
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ReversibleHooks {

// Cari base address library (.so) yang sudah di-load di proses ini.
// Dipakai reGTA.cpp: JNI_OnLoad -> FindLib("libGTASA.so").
inline uintptr_t FindLib(const char* libName)
{
    if (!libName || !libName[0])
        return 0;

    FILE* f = std::fopen("/proc/self/maps", "r");
    if (!f)
        return 0;

    uintptr_t base = 0;
    char line[1024];
    while (std::fgets(line, sizeof(line), f))
    {
        if (std::strstr(line, libName) == nullptr)
            continue;
        // Format baris: "start-end perms offset dev inode pathname"
        char* dash = std::strchr(line, '-');
        if (!dash)
            continue;
        *dash = '\0';
        uintptr_t start = (uintptr_t)std::strtoull(line, nullptr, 16);
        if (base == 0 || start < base)
            base = start; // ambil alamat pemetaan terendah = base
    }
    std::fclose(f);
    return base;
}

// Inisialisasi infrastruktur hook.
// STUB: no-op sampai injector asli dirilis upstream.
inline void InitHookStuff()
{
    // TODO: sambungkan ke InitHookStuff asli kalau sudah dirilis upstream.
}

// Redirect simbol (mis. "_ZN4CHID12GetInputTypeEv") ke fungsi pengganti.
// STUB: saat ini tidak melakukan apa-apa.
template <typename T>
inline void Redirect(const char* symbol, T /*replacement*/)
{
    (void)symbol;
    // TODO: sambungkan ke injector asli kalau sudah dirilis upstream.
}

} // namespace ReversibleHooks
