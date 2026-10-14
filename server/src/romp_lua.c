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

/* Registry untuk handler romp.onNUICallback(name, fn). */
static int nui_cb_ref = LUA_NOREF;

int lua_active(void) { return L != NULL; }

/* Helper frame kecil untuk romp_lua.c */
static void lput_u16(uint8_t *b, uint16_t v) { b[0] = v & 0xFF; b[1] = (v >> 8) & 0xFF; }

/* Escape string agar aman disisipkan sebagai nilai JSON (kutip ganda). */
static void json_escape(const char *s, char *out, size_t out_sz) {
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p && o + 6 < out_sz; p++) {
        if (*p == '"' || *p == '\\') { out[o++] = '\\'; out[o++] = (char)*p; }
        else if (*p == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else if (*p == '\r') { out[o++] = '\\'; out[o++] = 'r'; }
        else if (*p == '\t') { out[o++] = '\\'; out[o++] = 't'; }
        else if (*p < 0x20) {
            o += (size_t)snprintf(out + o, out_sz - o, "\\u%04x", *p);
        } else { out[o++] = (char)*p; }
    }
    out[o] = '\0';
}

/* Kirim NUI_MESSAGE mentah ke client. 1 = sukses, 0 = gagal. */
static int nui_send_raw(Client *c, const char *json) {
    size_t jl = strlen(json);
    if (jl == 0 || jl > 4000) return 0;
    uint8_t b[4100];
    b[0] = P_NUI_MSG;
    lput_u16(b + 1, (uint16_t)jl);
    memcpy(b + 3, json, jl);
    return client_send_frame(c, b, 3 + jl) == 0;
}

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

/* ---------------- API info & admin runtime ---------------- */

static int l_romp_get_player_ip(lua_State *Ls) {
    int id = (int)luaL_checkinteger(Ls, 1);
    Client *c = client_by_id(id);
    if (!c) { lua_pushnil(Ls); return 1; }
    lua_pushstring(Ls, c->ip);
    return 1;
}

static int l_romp_set_hostname(lua_State *Ls) {
    const char *s = luaL_checkstring(Ls, 1);
    strncpy(g_cfg.hostname, s, sizeof(g_cfg.hostname) - 1);
    g_cfg.hostname[sizeof(g_cfg.hostname) - 1] = '\0';
    return 0;
}

static int l_romp_set_motd(lua_State *Ls) {
    const char *s = luaL_checkstring(Ls, 1);
    strncpy(g_cfg.motd, s, sizeof(g_cfg.motd) - 1);
    g_cfg.motd[sizeof(g_cfg.motd) - 1] = '\0';
    return 0;
}

static int l_romp_set_password(lua_State *Ls) {
    const char *s = luaL_checkstring(Ls, 1);
    strncpy(g_cfg.password, s, sizeof(g_cfg.password) - 1);
    g_cfg.password[sizeof(g_cfg.password) - 1] = '\0';
    return 0;
}

static int l_romp_get_server_info(lua_State *Ls) {
    lua_newtable(Ls);
    lua_pushstring(Ls, g_cfg.hostname);   lua_setfield(Ls, -2, "hostname");
    lua_pushstring(Ls, g_cfg.motd);       lua_setfield(Ls, -2, "motd");
    lua_pushinteger(Ls, g_cfg.port);      lua_setfield(Ls, -2, "port");
    lua_pushinteger(Ls, g_cfg.maxplayers); lua_setfield(Ls, -2, "maxplayers");
    lua_pushinteger(Ls, client_count());   lua_setfield(Ls, -2, "players");
    lua_pushboolean(Ls, g_cfg.password[0] ? 1 : 0);
    lua_setfield(Ls, -2, "hasPassword");
    lua_pushboolean(Ls, g_cfg.query ? 1 : 0);
    lua_setfield(Ls, -2, "query");
    lua_pushboolean(Ls, g_cfg.announce ? 1 : 0);
    lua_setfield(Ls, -2, "announce");
    return 1;
}

static int l_romp_ban_ip(lua_State *Ls) {
    const char *ip = luaL_checkstring(Ls, 1);
    ban_add(ip);
    return 0;
}

static int l_romp_unban_ip(lua_State *Ls) {
    const char *ip = luaL_checkstring(Ls, 1);
    ban_remove(ip);
    return 0;
}

static int l_romp_is_ip_banned(lua_State *Ls) {
    const char *ip = luaL_checkstring(Ls, 1);
    lua_pushboolean(Ls, ban_check(ip));
    return 1;
}

static int l_romp_announce_now(lua_State *Ls) {
    (void)Ls;
    trigger_announce();
    return 0;
}

/* romp.getPlayerPos(id) -> x, y, z | nil (belum pernah kirim paket POS) */
static int l_romp_get_player_pos(lua_State *Ls) {
    int id = (int)luaL_checkinteger(Ls, 1);
    Client *c = client_by_id(id);
    if (!c || !c->pos_known) { lua_pushnil(Ls); return 1; }
    lua_pushnumber(Ls, (lua_Number)c->pos_x);
    lua_pushnumber(Ls, (lua_Number)c->pos_y);
    lua_pushnumber(Ls, (lua_Number)c->pos_z);
    return 3;
}

/* ---------------- API NUI (ala FiveM) ---------------- */

/* romp.sendNUIMessage(id, jsonString) -> true/false */
static int l_romp_send_nui_message(lua_State *Ls) {
    int id = (int)luaL_checkinteger(Ls, 1);
    size_t jl = 0;
    const char *json = luaL_checklstring(Ls, 2, &jl);
    Client *c = client_by_id(id);
    if (!c || jl == 0 || jl > 4000) { lua_pushboolean(Ls, 0); return 1; }
    uint8_t b[4096];
    b[0] = P_NUI_MSG;
    lput_u16(b + 1, (uint16_t)jl);
    memcpy(b + 3, json, jl);
    lua_pushboolean(Ls, client_send_frame(c, b, 3 + jl) == 0);
    return 1;
}

/* romp.onNUICallback(name, fn) — fn dipanggil sebagai fn(id, name, dataJson) */
static int l_romp_on_nui_callback(lua_State *Ls) {
    const char *name = luaL_checkstring(Ls, 1);
    luaL_checktype(Ls, 2, LUA_TFUNCTION);
    lua_rawgeti(Ls, LUA_REGISTRYINDEX, nui_cb_ref);
    lua_pushvalue(Ls, 2);
    lua_setfield(Ls, -2, name);
    lua_pop(Ls, 1);
    return 0;
}

/* romp.setNUIFocus(id, bool) -> true/false */
static int l_romp_set_nui_focus(lua_State *Ls) {
    int id = (int)luaL_checkinteger(Ls, 1);
    int focus = lua_toboolean(Ls, 2);
    Client *c = client_by_id(id);
    if (!c) { lua_pushboolean(Ls, 0); return 1; }
    uint8_t b[2] = { P_NUI_FOCUS, (uint8_t)(focus ? 1 : 0) };
    lua_pushboolean(Ls, client_send_frame(c, b, 2) == 0);
    return 1;
}

/* ---------------- API Dialog ala SA-MP ----------------
 * romp.showDialog(id, dialogId, style, caption, info, button1, button2)
 *   style: romp.DIALOG_MSGBOX(0) / DIALOG_INPUT(1) / DIALOG_LIST(2) /
 *          DIALOG_PASSWORD(3). info boleh multi-baris (\n).
 *   -> true/false. Otomatis setNUIFocus(id, true).
 * romp.hideDialog(id) -> true/false. Otomatis setNUIFocus(id, false).
 * Jawaban client datang sebagai callback NUI "__dialog_response" dan
 * di-routing dialog.lua ke event Lua:
 *   onDialogResponse(id, name, dialogId, response, listitem, inputtext)
 */
static int l_romp_show_dialog(lua_State *Ls) {
    int id = (int)luaL_checkinteger(Ls, 1);
    int dlg = (int)luaL_checkinteger(Ls, 2);
    int style = (int)luaL_checkinteger(Ls, 3);
    const char *caption = luaL_checkstring(Ls, 4);
    const char *info = luaL_checkstring(Ls, 5);
    const char *b1 = luaL_optstring(Ls, 6, "");
    const char *b2 = luaL_optstring(Ls, 7, "");
    Client *c = client_by_id(id);
    if (!c || style < 0 || style > 3) { lua_pushboolean(Ls, 0); return 1; }
    char ec[768], ei[2048], eb1[128], eb2[128];
    json_escape(caption, ec, sizeof ec);
    json_escape(info, ei, sizeof ei);
    json_escape(b1, eb1, sizeof eb1);
    json_escape(b2, eb2, sizeof eb2);
    char js[4096];
    int n = snprintf(js, sizeof js,
        "{\"action\":\"showDialog\",\"id\":%d,\"style\":%d,"
        "\"caption\":\"%s\",\"info\":\"%s\",\"button1\":\"%s\",\"button2\":\"%s\"}",
        dlg, style, ec, ei, eb1, eb2);
    /* Buffer di atas cukup untuk semua escape terpanjang, jadi n tak
     * mungkin terpotong; cek defensif saja. */
    if (n < 0 || (size_t)n >= sizeof js) { lua_pushboolean(Ls, 0); return 1; }
    if (!nui_send_raw(c, js)) { lua_pushboolean(Ls, 0); return 1; }
    uint8_t f[2] = { P_NUI_FOCUS, 1 };
    lua_pushboolean(Ls, client_send_frame(c, f, 2) == 0);
    return 1;
}

static int l_romp_hide_dialog(lua_State *Ls) {
    int id = (int)luaL_checkinteger(Ls, 1);
    Client *c = client_by_id(id);
    if (!c) { lua_pushboolean(Ls, 0); return 1; }
    nui_send_raw(c, "{\"action\":\"hideDialog\"}");
    uint8_t f[2] = { P_NUI_FOCUS, 0 };
    lua_pushboolean(Ls, client_send_frame(c, f, 2) == 0);
    return 1;
}

/* ---------------- Init ---------------- */

/* Dipakai lua_init sebelum definisinya (di bawah). */
static void call_event_noret(const char *ev);

/* Muat helper Lua (dialog.lua, textdraw.lua, ...) dari folder yang sama
 * dengan file gamemode. Tidak fatal kalau file tidak ada. */
static void lua_load_helper(const char *script_path, const char *file,
                            const char *desc) {
    char dir[512];
    const char *slash = strrchr(script_path, '/');
#ifdef _WIN32
    const char *bs = strrchr(script_path, '\\');
    if (bs && (!slash || bs > slash)) slash = bs;
#endif
    if (slash) {
        size_t dl = (size_t)(slash - script_path);
        if (dl > sizeof dir - 16) dl = sizeof dir - 16;
        memcpy(dir, script_path, dl);
        dir[dl] = '\0';
    } else {
        dir[0] = '.'; dir[1] = '\0';
    }
    char path[528];
    snprintf(path, sizeof path, "%s/%s", dir, file);
    if (luaL_loadfile(L, path) != LUA_OK || lua_pcall(L, 0, 0, 0) != LUA_OK) {
        log_msg("[lua] %s tidak dimuat (%s): %s", file, path,
                lua_tostring(L, -1) ? lua_tostring(L, -1) : "?");
        lua_pop(L, 1);
    } else {
        log_msg("[lua] %s dimuat (%s)", file, desc);
    }
}

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
    lua_pushcfunction(L, l_romp_get_player_pos);  lua_setfield(L, -2, "getPlayerPos");
    lua_pushcfunction(L, l_romp_send_chat_to_all); lua_setfield(L, -2, "sendChatToAll");
    lua_pushcfunction(L, l_romp_send_chat_to_player); lua_setfield(L, -2, "sendChatToPlayer");
    lua_pushcfunction(L, l_romp_kick_player);      lua_setfield(L, -2, "kickPlayer");
    lua_pushcfunction(L, l_romp_get_tick);         lua_setfield(L, -2, "getTick");
    lua_pushcfunction(L, l_romp_get_config);       lua_setfield(L, -2, "getConfig");
    lua_pushcfunction(L, l_romp_send_nui_message); lua_setfield(L, -2, "sendNUIMessage");
    lua_pushcfunction(L, l_romp_on_nui_callback);  lua_setfield(L, -2, "onNUICallback");
    lua_pushcfunction(L, l_romp_set_nui_focus);    lua_setfield(L, -2, "setNUIFocus");
    lua_pushcfunction(L, l_romp_show_dialog);      lua_setfield(L, -2, "showDialog");
    lua_pushcfunction(L, l_romp_hide_dialog);      lua_setfield(L, -2, "hideDialog");
    lua_pushcfunction(L, l_romp_get_player_ip);   lua_setfield(L, -2, "getPlayerIP");
    lua_pushcfunction(L, l_romp_set_hostname);    lua_setfield(L, -2, "setHostname");
    lua_pushcfunction(L, l_romp_set_motd);        lua_setfield(L, -2, "setMOTD");
    lua_pushcfunction(L, l_romp_set_password);    lua_setfield(L, -2, "setPassword");
    lua_pushcfunction(L, l_romp_get_server_info); lua_setfield(L, -2, "getServerInfo");
    lua_pushcfunction(L, l_romp_ban_ip);          lua_setfield(L, -2, "banIP");
    lua_pushcfunction(L, l_romp_unban_ip);        lua_setfield(L, -2, "unbanIP");
    lua_pushcfunction(L, l_romp_is_ip_banned);    lua_setfield(L, -2, "isIPBanned");
    lua_pushcfunction(L, l_romp_announce_now);    lua_setfield(L, -2, "announceNow");
    /* konstanta kind chat */
    lua_pushinteger(L, KIND_CHAT); lua_setfield(L, -2, "KIND_CHAT");
    lua_pushinteger(L, KIND_INFO); lua_setfield(L, -2, "KIND_INFO");
    lua_pushinteger(L, KIND_ME);   lua_setfield(L, -2, "KIND_ME");
    lua_pushinteger(L, KIND_PM);   lua_setfield(L, -2, "KIND_PM");
    lua_pushinteger(L, KIND_SYS);  lua_setfield(L, -2, "KIND_SYS");
    /* konstanta style dialog ala SA-MP */
    lua_pushinteger(L, 0); lua_setfield(L, -2, "DIALOG_MSGBOX");
    lua_pushinteger(L, 1); lua_setfield(L, -2, "DIALOG_INPUT");
    lua_pushinteger(L, 2); lua_setfield(L, -2, "DIALOG_LIST");
    lua_pushinteger(L, 3); lua_setfield(L, -2, "DIALOG_PASSWORD");
    lua_setglobal(L, "romp");

    /* tabel registry untuk NUI callback */
    lua_newtable(L);
    nui_cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    if (luaL_loadfile(L, script_path) != LUA_OK ||
        lua_pcall(L, 0, 0, 0) != LUA_OK) {
        log_msg("[lua] gagal load '%s': %s", script_path,
                lua_tostring(L, -1) ? lua_tostring(L, -1) : "?");
        lua_close(L);
        L = NULL;
        return -1;
    }
    log_msg("[lua] gamemode dimuat: %s", script_path);
    /* helper ala SA-MP: router dialog + API textdraw */
    lua_load_helper(script_path, "dialog.lua", "event onDialogResponse aktif");
    lua_load_helper(script_path, "textdraw.lua", "API textdraw aktif");
    lua_load_helper(script_path, "checkpoint.lua", "API checkpoint aktif");
    lua_load_helper(script_path, "pickup.lua", "API pickup aktif");
    lua_load_helper(script_path, "mapicon.lua", "API map icon aktif");
    /* event init: onGamemodeInit() */
    call_event_noret("onGamemodeInit");
    return 0;
}

void lua_shutdown(void) {
    if (L) { lua_close(L); L = NULL; }
    nui_cb_ref = LUA_NOREF;
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

/* onConsoleCommand(cmd, args) -> true = di-handle gamemode. */
typedef struct { const char *cmd, *args; } EvCon;
static int push_con(lua_State *Ls, void *ud) {
    EvCon *e = (EvCon*)ud;
    lua_pushstring(Ls, e->cmd);
    lua_pushstring(Ls, e->args);
    return 2;
}
int lua_on_console_command(const char *cmd, const char *args) {
    EvCon e = { cmd, args };
    return call_event("onConsoleCommand", push_con, &e);
}

/* onPlayerRequestConnect(name, ip) -> true, "alasan" = tolak koneksi. */
int lua_on_player_request(const char *name, const char *ip,
                          char *reason_out, size_t reason_sz) {
    if (!L) return 0;
    lua_getglobal(L, "onPlayerRequestConnect");
    if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return 0; }
    lua_pushstring(L, name);
    lua_pushstring(L, ip);
    if (lua_pcall(L, 2, 2, 0) != LUA_OK) {
        log_msg("[lua] error di onPlayerRequestConnect: %s",
                lua_tostring(L, -1) ? lua_tostring(L, -1) : "?");
        lua_pop(L, 1);
        return 0;
    }
    int reject = lua_toboolean(L, -2);
    if (reject && lua_isstring(L, -1) && reason_out && reason_sz > 0) {
        strncpy(reason_out, lua_tostring(L, -1), reason_sz - 1);
        reason_out[reason_sz - 1] = '\0';
    }
    lua_pop(L, 2);
    return reject;
}

/* Dispatch NUI callback dari client ke handler Lua terdaftar. */
int lua_on_nui_callback(int id, const char *name, const char *data) {
    if (!L) return 0;
    lua_rawgeti(L, LUA_REGISTRYINDEX, nui_cb_ref);
    lua_getfield(L, -1, name);
    lua_remove(L, -2); /* sisakan fn */
    if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return 0; }
    lua_pushinteger(L, id);
    lua_pushstring(L, name);
    lua_pushstring(L, data);
    if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
        log_msg("[lua] error di nui callback '%s': %s", name,
                lua_tostring(L, -1) ? lua_tostring(L, -1) : "?");
        lua_pop(L, 1);
    }
    return 1;
}

void lua_on_tick(void) {
    call_event_noret("onTick");
}
