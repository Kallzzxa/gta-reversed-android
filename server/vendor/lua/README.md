# Lua 5.4.7 (vendored)

Source Lua 5.4.7 dari https://www.lua.org/ftp/lua-5.4.7.tar.gz, hanya file
`src/*.c` + `src/*.h` (tanpa `lua.c`/`luac.c` yang berisi `main`).

Lisensi: MIT — lihat https://www.lua.org/license.html

Dipakai oleh scripting layer gamemode (`src/romp_lua.c`), di-link statis ke
binari server Linux & Windows.
