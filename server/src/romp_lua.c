/*
 * RO-MP scripting layer — implementasi.
 * Semua pemanggilan Lua hanya dari main thread server.
 */
#include "romp_lua.h"
#include "romp_server.h"

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

static lua_State *L = NULL;

int lua_active(void) { return L != NULL; }

/* ---------------- API romp.* untuk gamemode ---------------- */

static int l_romp_log(lua_State *Ls) {
    const char *msg = luaL_checkstring(Ls, 1);
    log_msg("[lua] %s", msg);
    return 0;
}

static int l_romp_player_count(lua_State *Ls) {
    lua_pushinteger(Ls, client_count());
    return 1;
}

static int l_romp_get_player_name(lua_State *Ls) {
    int id = (int)luaL_checkinteger(Ls, 1);
    Client *c = client_by_id(id);
    if (c) lua_pushstring(Ls, c->name);
    else lua_pushnil(Ls);
    return 1;
}

static int l_romp_get_players(lua_State *Ls) {
    lua_newtable(Ls);
    int i = 1;
    for (Client *c = g_clients; c; c = c->next) {
        if (!c->handshaked || c->dead) continue;
        lua_newtable(Ls);
        lua_pushinteger(Ls, c->id); lua_setfield(Ls, -2, "id");
        lua_pushstring(Ls, c->name); lua_setfield(Ls, -2, "name");
        lua_rawseti(Ls, -2, i++);
    }
    return 1;
}

static int l_romp_send_chat_to_all(lua_State *Ls) {
    int kind = (int)luaL_checkinteger(Ls, 1);
    const char *text = luaL_checkstring(Ls, 2);
    if (kind < 0 || kind > 4) kind = KIND_CHAT;
    chat_broadcast(CHAT_SERVER, (uint8_t)kind, text);
    return 0;
}

static int l_romp_send_chat_to_player(lua_State *Ls) {
    int id = (int)luaL_checkinteger(Ls, 1);
    int kind = (int)luaL_checkinteger(Ls, 2);
    const char *text = luaL_checkstring(Ls, 3);
    Client *c = client_by_id(id);
    if (!c) { lua_pushboolean(Ls, 0); return 1; }
    if (kind < 0 || kind > 4) kind = KIND_CHAT;
    chat_to(c, CHAT_SERVER, (uint8_t)kind, text);
    lua_pushboolean(Ls, 1);
    return 1;
}

static int l_romp_kick_player(lua_State *Ls) {
    int id = (int)luaL_checkinteger(Ls, 1);
    const char *reason = luaL_optstring(Ls, 2, "");
    server_kick(id, reason);
    return 0;
}

static int l_romp_get_tick(lua_State *Ls) {
    lua_pushinteger(Ls, (lua_Integer)now_ms());
    return 1;
}

static int l_romp_get_config(lua_State *Ls) {
    const char *key = luaL_checkstring(Ls, 1);
    if (!strcmp(key, "hostname")) lua_pushstring(Ls, g_cfg.hostname);
    else if (!strcmp(key, "motd")) lua_pushstring(Ls, g_cfg.motd);
    else if (!strcmp(key, "port")) lua_pushinteger(Ls, g_cfg.port);
    else if (!strcmp(key, "maxplayers")) lua_pushinteger(Ls, g_cfg.maxplayers);
    else lua_pushnil(Ls);
    return 1;
}

/* ---------------- Init ---------------- */

int lua_init(const char *script_path) {
    L = luaL_newstate();
    if (!L) return -1;
    luaL_openlibs(L);

    /* tabel romp */
    lua_newtable(L);
    lua_pushcfunction(L, l_romp_log);              lua_setfield(L, -2, "log");
    lua_pushcfunction(L, l_romp_player_count);     lua_setfield(L, -2, "playerCount");
    lua_pushcfunction(L, l_romp_get_player_name);  lua_setfield(L, -2, "getPlayerName");
    lua_pushcfunction(L, l_romp_get_players);      lua_setfield(L, -2, "getPlayers");
    lua_pushcfunction(L, l_romp_send_chat_to_all); lua_setfield(L, -2, "sendChatToAll");
    lua_pushcfunction(L, l_romp_send_chat_to_player); lua_setfield(L, -2, "sendChatToPlayer");
    lua_pushcfunction(L, l_romp_kick_player);      lua_setfield(L, -2, "kickPlayer");
    lua_pushcfunction(L, l_romp_get_tick);         lua_setfield(L, -2, "getTick");
    lua_pushcfunction(L, l_romp_get_config);       lua_setfield(L, -2, "getConfig");
    /* konstanta kind chat */
    lua_pushinteger(L, KIND_CHAT); lua_setfield(L, -2, "KIND_CHAT");
    lua_pushinteger(L, KIND_INFO); lua_setfield(L, -2, "KIND_INFO");
    lua_pushinteger(L, KIND_ME);   lua_setfield(L, -2, "KIND_ME");
    lua_pushinteger(L, KIND_PM);   lua_setfield(L, -2, "KIND_PM");
    lua_pushinteger(L, KIND_SYS);  lua_setfield(L, -2, "KIND_SYS");
    lua_setglobal(L, "romp");

    if (luaL_loadfile(L, script_path) != LUA_OK ||
        lua_pcall(L, 0, 0, 0) != LUA_OK) {
        log_msg("[lua] gagal load '%s': %s", script_path,
                lua_tostring(L, -1) ? lua_tostring(L, -1) : "?");
        lua_close(L);
        L = NULL;
        return -1;
    }
    log_msg("[lua] gamemode dimuat: %s", script_path);
    return 0;
}

void lua_shutdown(void) {
    if (L) { lua_close(L); L = NULL; }
}

/* ---------------- Dispatch event ---------------- */

/* Panggil fungsi global Lua `ev`. push_args mendorong argumen & mengembalikan
 * jumlahnya. Kembalikan 1 jika fungsi ada & return true (handled). */
typedef int (*push_args_fn)(lua_State *Ls, void *ud);

static int call_event(const char *ev, push_args_fn push, void *ud) {
    if (!L) return 0;
    lua_getglobal(L, ev);
    if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return 0; }
    int nargs = push(L, ud);
    if (lua_pcall(L, nargs, 1, 0) != LUA_OK) {
        log_msg("[lua] error di %s: %s", ev,
                lua_tostring(L, -1) ? lua_tostring(L, -1) : "?");
        lua_pop(L, 1);
        return 0;
    }
    int handled = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return handled;
}

static void call_event_noret(const char *ev) {
    if (!L) return;
    lua_getglobal(L, ev);
    if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        log_msg("[lua] error di %s: %s", ev,
                lua_tostring(L, -1) ? lua_tostring(L, -1) : "?");
        lua_pop(L, 1);
    }
}

typedef struct { int id; const char *name; } EvJoin;
static int push_join(lua_State *Ls, void *ud) {
    EvJoin *e = (EvJoin*)ud;
    lua_pushinteger(Ls, e->id);
    lua_pushstring(Ls, e->name);
    return 2;
}

typedef struct { int id; const char *name; int reason; } EvLeave;
static int push_leave(lua_State *Ls, void *ud) {
    EvLeave *e = (EvLeave*)ud;
    lua_pushinteger(Ls, e->id);
    lua_pushstring(Ls, e->name);
    lua_pushinteger(Ls, e->reason);
    return 3;
}

typedef struct { int id; const char *name; const char *text; } EvChat;
static int push_chat(lua_State *Ls, void *ud) {
    EvChat *e = (EvChat*)ud;
    lua_pushinteger(Ls, e->id);
    lua_pushstring(Ls, e->name);
    lua_pushstring(Ls, e->text);
    return 3;
}

typedef struct { int id; const char *name; const char *cmd; const char *args; } EvCmd;
static int push_cmd(lua_State *Ls, void *ud) {
    EvCmd *e = (EvCmd*)ud;
    lua_pushinteger(Ls, e->id);
    lua_pushstring(Ls, e->name);
    lua_pushstring(Ls, e->cmd);
    lua_pushstring(Ls, e->args);
    return 4;
}

int lua_on_player_join(int id, const char *name) {
    EvJoin e = { id, name };
    return call_event("onPlayerJoin", push_join, &e);
}

int lua_on_player_leave(int id, const char *name, int reason) {
    EvLeave e = { id, name, reason };
    return call_event("onPlayerLeave", push_leave, &e);
}

int lua_on_player_chat(int id, const char *name, const char *text) {
    EvChat e = { id, name, text };
    return call_event("onPlayerChat", push_chat, &e);
}

int lua_on_player_command(int id, const char *name, const char *cmd, const char *args) {
    EvCmd e = { id, name, cmd, args };
    return call_event("onPlayerCommand", push_cmd, &e);
}

void lua_on_tick(void) {
    call_event_noret("onTick");
}
