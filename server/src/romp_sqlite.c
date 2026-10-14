/*
 * RO-MP SQLite wrapper ala SA-MP — implementasi.
 * Lihat romp_sqlite.h untuk dokumentasi API.
 */
#include "romp_sqlite.h"

#include <string.h>
#include <stdio.h>

#include "../vendor/sqlite/sqlite3.h"

static sqlite3 *s_db[DB_MAX_HANDLES];
static int      s_default = -1;
static char     s_open_err[256];

int db_open(const char *path) {
    s_open_err[0] = '\0';
    if (!path || !path[0]) {
        strncpy(s_open_err, "path database kosong", sizeof s_open_err - 1);
        return -1;
    }
    int h = -1;
    for (int i = 0; i < DB_MAX_HANDLES; i++)
        if (!s_db[i]) { h = i; break; }
    if (h < 0) {
        strncpy(s_open_err, "slot database penuh", sizeof s_open_err - 1);
        return -1;
    }
    sqlite3 *db = NULL;
    int rc = sqlite3_open_v2(path, &db,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                             SQLITE_OPEN_FULLMUTEX, NULL);
    if (rc != SQLITE_OK) {
        snprintf(s_open_err, sizeof s_open_err, "%s",
                 db ? sqlite3_errmsg(db) : "sqlite3_open gagal");
        if (db) sqlite3_close(db);
        return -1;
    }
    /* Mode aman untuk game server: tulis sinkron penuh, tunggu lock. */
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    sqlite3_busy_timeout(db, 5000);
    s_db[h] = db;
    return h;
}

void db_close(int h) {
    if (h < 0 || h >= DB_MAX_HANDLES || !s_db[h]) return;
    sqlite3_close(s_db[h]);
    s_db[h] = NULL;
    if (s_default == h) s_default = -1;
}

void db_close_all(void) {
    for (int i = 0; i < DB_MAX_HANDLES; i++) db_close(i);
    s_default = -1;
}

const char *db_last_error(int h) {
    if (h < 0) return s_open_err;
    if (h >= DB_MAX_HANDLES || !s_db[h]) return "";
    return sqlite3_errmsg(s_db[h]);
}

int db_get_default(void) { return s_default; }

void db_set_default(int h) {
    if (h >= 0 && h < DB_MAX_HANDLES && s_db[h]) s_default = h;
    else s_default = -1;
}

struct sqlite3 *db_raw(int h) {
    if (h < 0 || h >= DB_MAX_HANDLES) return NULL;
    return s_db[h];
}
