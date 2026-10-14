/*
 * RO-MP SQLite wrapper ala SA-MP (mirip plugin sqlite / db_*).
 *
 * Menyediakan database file lokal untuk gamemode Lua (skor, akun, dsb).
 * SQLite dipilih karena tanpa server eksternal: cukup satu file .db
 * (dibanding MySQL yang butuh server + libmysqlclient terpisah).
 *
 * Handle = integer kecil (0..DB_MAX_HANDLES-1), dibagikan ke semua
 * state Lua (gamemode + filterscript). Satu handle default bisa dibuka
 * otomatis dari server.cfg ("database <path>") dan diakses Lua sebagai
 * romp.db. Jangan tutup romp.db dari filterscript.
 */
#pragma once

struct sqlite3;

#define DB_MAX_HANDLES 8

/* Buka database. Return >= 0 = handle, -1 = gagal
 * (pesan via db_last_error(-1)). Path relatif = relatif ke CWD server. */
int db_open(const char *path);

/* Tutup satu handle (aman dipanggil dengan handle invalid).
 * Menutup handle default juga mengosongkan default. */
void db_close(int h);

/* Tutup semua handle (dipakai saat shutdown server). */
void db_close_all(void);

/* Pesan error terakhir. h < 0 = error db_open terakhir,
 * handle invalid = "". */
const char *db_last_error(int h);

/* Handle default dari server.cfg, -1 bila tidak dikonfigurasi. */
int  db_get_default(void);
void db_set_default(int h);

/* Handle mentah untuk API sqlite3 langsung. NULL bila invalid. */
struct sqlite3 *db_raw(int h);
