/*
 * Timer Lua ala SA-MP SetTimer/SetTimerEx/KillTimer.
 *
 *   local id = romp.setTimer(cb, ms, repeating, ...)
 *   romp.killTimer(id)
 *
 * cb = fungsi Lua atau nama fungsi global; argumen tambahan diteruskan
 * ke callback seperti SetTimerEx (tanpa format string, posisi saja).
 * Semua pemanggilan hanya dari main thread server.
 */
#include "romp_timer.h"
#include "romp_server.h" /* log_msg, now_ms */

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include <stdlib.h>
#include <string.h>

#define MAX_TIMERS   256
#define TIMER_MIN_MS 50ULL
#define TIMER_MAX_MS 86400000ULL /* 24 jam */

typedef struct Timer {
    int id;
    lua_State *L;      /* state pemilik (gamemode / filterscript) */
    int cb_ref;        /* registry: fungsi callback */
    int args_ref;      /* registry: tabel array argumen */
    uint64_t interval;
    int repeating;
    uint64_t next_fire;
    struct Timer *next;
} Timer;

static Timer *g_timers = NULL;
static int g_count = 0;
static int g_next_id = 1;

static Timer *timer_find(int id) {
    for (Timer *t = g_timers; t; t = t->next)
        if (t->id == id) return t;
    return NULL;
}

/* romp.setTimer(cb, ms, repeating, ...) -> timerid | nil, errmsg */
int l_romp_set_timer(lua_State *L) {
    int n = lua_gettop(L);
    if (n < 3) {
        lua_pushnil(L);
        lua_pushstring(L, "usage: romp.setTimer(callback, ms, repeating, ...)");
        return 2;
    }

    int cb_ref;
    if (lua_isfunction(L, 1)) {
        lua_pushvalue(L, 1);
        cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    } else if (lua_isstring(L, 1)) {
        lua_getglobal(L, lua_tostring(L, 1));
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 1);
            lua_pushnil(L);
            lua_pushstring(L, "callback global tidak ditemukan");
            return 2;
        }
        cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    } else {
        lua_pushnil(L);
        lua_pushstring(L, "callback harus fungsi atau nama fungsi global");
        return 2;
    }

    long long ms = (long long)luaL_optinteger(L, 2, 0);
    int repeating = lua_toboolean(L, 3);
    if (ms < (long long)TIMER_MIN_MS || ms > (long long)TIMER_MAX_MS) {
        luaL_unref(L, LUA_REGISTRYINDEX, cb_ref);
        lua_pushnil(L);
        lua_pushstring(L, "interval harus 50..86400000 ms");
        return 2;
    }
    if (g_count >= MAX_TIMERS) {
        luaL_unref(L, LUA_REGISTRYINDEX, cb_ref);
        lua_pushnil(L);
        lua_pushstring(L, "terlalu banyak timer aktif (maks 256)");
        return 2;
    }

    /* Simpan argumen sisa (arg #4 ke atas) sebagai tabel array. */
    lua_newtable(L);
    for (int i = 4; i <= n; i++) {
        lua_pushvalue(L, i);
        lua_rawseti(L, -2, (lua_Integer)(i - 3));
    }
    int args_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    Timer *t = (Timer *)calloc(1, sizeof(Timer));
    if (!t) {
        luaL_unref(L, LUA_REGISTRYINDEX, cb_ref);
        luaL_unref(L, LUA_REGISTRYINDEX, args_ref);
        lua_pushnil(L);
        lua_pushstring(L, "kehabisan memori");
        return 2;
    }
    int id = g_next_id;
    while (timer_find(id)) { /* jaga-jaga tabrakan id */
        id++;
        if (id < 0) id = 1;
    }
    g_next_id = id + 1;
    if (g_next_id < 0) g_next_id = 1;

    t->id = id;
    t->L = L;
    t->cb_ref = cb_ref;
    t->args_ref = args_ref;
    t->interval = (uint64_t)ms;
    t->repeating = repeating;
    t->next_fire = now_ms() + t->interval;
    t->next = g_timers;
    g_timers = t;
    g_count++;

    lua_pushinteger(L, id);
    return 1;
}

/* romp.killTimer(id) -> true/false */
int l_romp_kill_timer(lua_State *L) {
    int id = (int)luaL_checkinteger(L, 1);
    Timer **pp = &g_timers;
    while (*pp) {
        Timer *t = *pp;
        if (t->id == id) {
            *pp = t->next;
            luaL_unref(t->L, LUA_REGISTRYINDEX, t->cb_ref);
            luaL_unref(t->L, LUA_REGISTRYINDEX, t->args_ref);
            free(t);
            g_count--;
            lua_pushboolean(L, 1);
            return 1;
        }
        pp = &t->next;
    }
    lua_pushboolean(L, 0);
    return 1;
}

/* romp.getTimerCount() -> jumlah timer aktif */
int l_romp_get_timer_count(lua_State *L) {
    lua_pushinteger(L, g_count);
    return 1;
}

/* Hapus semua timer milik state L (dipanggil sebelum lua_close). */
void timer_kill_state(lua_State *L) {
    Timer **pp = &g_timers;
    while (*pp) {
        Timer *t = *pp;
        if (t->L == L) {
            *pp = t->next;
            luaL_unref(t->L, LUA_REGISTRYINDEX, t->cb_ref);
            luaL_unref(t->L, LUA_REGISTRYINDEX, t->args_ref);
            free(t);
            g_count--;
        } else {
            pp = &t->next;
        }
    }
}

static void timer_fire(Timer *t) {
    lua_State *L = t->L;
    int base = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, t->cb_ref);   /* [cb] */
    lua_rawgeti(L, LUA_REGISTRYINDEX, t->args_ref); /* [cb, args] */
    int args_idx = base + 2; /* indeks absolut tabel args (stabil) */
    size_t n = lua_rawlen(L, args_idx);
    for (size_t i = 1; i <= n; i++)
        lua_rawgeti(L, args_idx, (lua_Integer)i);   /* [cb, args, a1..an] */
    lua_remove(L, args_idx);                        /* [cb, a1..an] */
    if (lua_pcall(L, (int)n, 0, 0) != LUA_OK) {
        log_msg("[lua] error di timer %d: %s", t->id,
                lua_tostring(L, -1) ? lua_tostring(L, -1) : "?");
        lua_pop(L, 1);
    }
}

void timer_tick(uint64_t now) {
    if (!g_timers) return;

    /* Kumpulkan dulu yang jatuh tempo: callback boleh membuat/membunuh
     * timer, termasuk timernya sendiri. Maks 64 per tick agar satu tick
     * berat tak menahan loop terlalu lama. */
    Timer *due[64];
    int nd = 0;
    Timer **pp = &g_timers;
    while (*pp && nd < 64) {
        Timer *t = *pp;
        if ((int64_t)(now - t->next_fire) >= 0) {
            *pp = t->next; /* cabut dari list sebelum fire */
            g_count--;
            due[nd++] = t;
        } else {
            pp = &t->next;
        }
    }

    for (int i = 0; i < nd; i++) {
        Timer *t = due[i];
        if (t->repeating) {
            /* Masukkan lagi SEBELUM fire: callback boleh killTimer(id)
             * terhadap dirinya sendiri. */
            t->next_fire += t->interval;
            if ((int64_t)(now - t->next_fire) > 0)
                t->next_fire = now + t->interval; /* kejar ketertinggalan */
            t->next = g_timers;
            g_timers = t;
            g_count++;
        }
        timer_fire(t);
        if (!t->repeating) {
            luaL_unref(t->L, LUA_REGISTRYINDEX, t->cb_ref);
            luaL_unref(t->L, LUA_REGISTRYINDEX, t->args_ref);
            free(t);
        }
    }
}
