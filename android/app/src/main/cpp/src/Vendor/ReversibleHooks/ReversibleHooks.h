//
// ReversibleHooks.h — STUB KOMPATIBILITAS LOKAL
//
// Header asli ReversibleHooks.h dirahasiakan oleh upstream
// (lihat .gitignore: "# Private ReversibleHooks"), sehingga repo ini
// tidak bisa di-build tanpa file ini.
//
// File ini adalah stub clean-room: API-nya meniru yang dipakai oleh
// Hooks.cpp / InjectReversedClasses.cpp, tapi implementasinya no-op.
// Kalau upstream merilis header aslinya, GANTI file ini dengan yang asli.
//
// Dibuat untuk membuat `gradlew assembleDebug` jalan out-of-the-box.
//
#pragma once

namespace ReversibleHooks {

// Redirect simbol (mis. "_ZN4CHID12GetInputTypeEv") ke fungsi pengganti.
// STUB: saat ini tidak melakukan apa-apa. Implementasi injector yang
// sebenarnya ("Our injector will soon be available") belum ada di hulu.
template <typename T>
inline void Redirect(const char* symbol, T /*replacement*/)
{
    (void)symbol;
    // TODO: sambungkan ke injector asli kalau sudah dirilis upstream.
}

} // namespace ReversibleHooks
