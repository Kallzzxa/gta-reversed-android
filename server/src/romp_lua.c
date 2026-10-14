/*
 * RO-MP scripting layer — implementasi.
 * Semua pemanggilan Lua hanya dari main thread server.
 */
#include "romp_lua.h"
#include "romp_server.h"
#include "romp_vehicle.h"
#include "romp_weapon.h"

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

static Script g_gm;              /* gamemode: state Lua utama */
static Script g_fs[MAX_FILTERSCRIPTS]; /* filterscript ala SA-MP */
static char g_gm_dir[512] = "";  /* folder gamemode (tempat helper utama) */
static char g_fs_dir[512] = "";  /* folder filterscripts = <gm_dir>/filterscripts */

/* State yang menunggu di-close (unload dari dalam event sendiri aman:
 * close ditunda sampai tick berikutnya, di luar pcall). */
static lua_State *g_pending_close[MAX_FILTERSCRIPTS];
static int g_pending_n = 0;

int lua_active(void) { return g_gm.L != NULL; }

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
        lua_pushinteger(Ls, c->interior); lua_setfield(Ls, -2, "interior");
        lua_pushinteger(Ls, (lua_Integer)c->vw); lua_setfield(Ls, -2, "vw");
        lua_pushboolean(Ls, c->is_npc); lua_setfield(Ls, -2, "isnpc");
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

/* romp.onNUICallback(name, fn) — fn dipanggil sebagai fn(id, name, dataJson).
 * Tabel handler disimpan per-state (tiap modul punya registry sendiri). */
static int l_romp_on_nui_callback(lua_State *Ls) {
    const char *name = luaL_checkstring(Ls, 1);
    luaL_checktype(Ls, 2, LUA_TFUNCTION);
    lua_getfield(Ls, LUA_REGISTRYINDEX, "romp.nui_cb_ref");
    int ref = (int)lua_tointeger(Ls, -1);
    lua_pop(Ls, 1);
    lua_rawgeti(Ls, LUA_REGISTRYINDEX, ref);
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

/* ---------------- API Kendaraan ala SA-MP ----------------
 * romp.createVehicle(model, x, y, z, angle, c1, c2, respawnDelayMs) -> vid|nil
 *   model: id model SA-MP 400..611. respawnDelayMs 0 = tanpa respawn otomatis.
 * romp.destroyVehicle(vid) -> bool
 * romp.setVehiclePos(vid, x, y, z [, angle]) -> bool
 * romp.getVehiclePos(vid) -> x, y, z, angle | nil
 * romp.setVehicleHealth(vid, health [, killerid]) -> bool   (health 0..1000)
 * romp.getVehicleHealth(vid) -> number | nil
 * romp.damageVehicle(vid, amount [, killerid]) -> health baru | nil
 * romp.putPlayerInVehicle(pid, vid, seat) -> bool   (seat 0=sopir, 1..3)
 * romp.removePlayerFromVehicle(pid) -> bool
 * romp.isPlayerInVehicle(pid) -> bool
 * romp.getPlayerVehicle(pid) -> vid | nil
 * romp.getPlayerVehicleSeat(pid) -> seat | nil
 * romp.getVehicleModel(vid) -> model | nil
 * romp.getVehicleColors(vid) -> c1, c2 | nil
 * romp.setVehicleColors(vid, c1, c2) -> bool
 * romp.getVehicleDriver(vid) -> pid | nil
 * romp.getVehiclePassenger(vid, seat) -> pid | nil
 * romp.respawnVehicle(vid) -> bool
 * romp.getVehicles() -> tabel {id,model,x,y,z,heading,c1,c2,health,
 *                              destroyed,driver,seats[0..3]}
 * Event: onPlayerEnterVehicle(pid, vid, seat), onPlayerExitVehicle(pid, vid),
 *        onVehicleDamage(vid, damagerid, newhealth),
 *        onVehicleDeath(vid, killerid), onVehicleSpawn(vid)
 */

static int l_romp_create_vehicle(lua_State *Ls) {
    uint16_t model = (uint16_t)luaL_checkinteger(Ls, 1);
    float x = (float)luaL_checknumber(Ls, 2);
    float y = (float)luaL_checknumber(Ls, 3);
    float z = (float)luaL_checknumber(Ls, 4);
    float angle = (float)luaL_optnumber(Ls, 5, 0.0);
    uint8_t c1 = (uint8_t)luaL_optinteger(Ls, 6, 0);
    uint8_t c2 = (uint8_t)luaL_optinteger(Ls, 7, 0);
    lua_Integer rd = luaL_optinteger(Ls, 8, 0);
    if (rd < 0) rd = 0;
    int vid = vehicle_create(model, x, y, z, angle, c1, c2, (uint32_t)rd);
    if (vid < 0) { lua_pushnil(Ls); return 1; }
    lua_pushinteger(Ls, vid);
    return 1;
}

static int l_romp_destroy_vehicle(lua_State *Ls) {
    int vid = (int)luaL_checkinteger(Ls, 1);
    lua_pushboolean(Ls, vehicle_destroy(vid));
    return 1;
}

static int l_romp_set_vehicle_pos(lua_State *Ls) {
    int vid = (int)luaL_checkinteger(Ls, 1);
    float x = (float)luaL_checknumber(Ls, 2);
    float y = (float)luaL_checknumber(Ls, 3);
    float z = (float)luaL_checknumber(Ls, 4);
    Vehicle *v = vehicle_by_id(vid);
    float angle = v ? v->heading : 0.0f;
    if (lua_gettop(Ls) >= 5) angle = (float)luaL_checknumber(Ls, 5);
    lua_pushboolean(Ls, vehicle_set_pos(vid, x, y, z, angle));
    return 1;
}

static int l_romp_get_vehicle_pos(lua_State *Ls) {
    int vid = (int)luaL_checkinteger(Ls, 1);
    Vehicle *v = vehicle_by_id(vid);
    if (!v) { lua_pushnil(Ls); return 1; }
    lua_pushnumber(Ls, (lua_Number)v->x);
    lua_pushnumber(Ls, (lua_Number)v->y);
    lua_pushnumber(Ls, (lua_Number)v->z);
    lua_pushnumber(Ls, (lua_Number)v->heading);
    return 4;
}

static int l_romp_set_vehicle_health(lua_State *Ls) {
    int vid = (int)luaL_checkinteger(Ls, 1);
    lua_Integer h = luaL_checkinteger(Ls, 2);
    int killer = (int)luaL_optinteger(Ls, 3, -1);
    if (h < 0) h = 0;
    if (h > VEH_HEALTH_MAX) h = VEH_HEALTH_MAX;
    lua_pushboolean(Ls, vehicle_set_health(vid, (uint16_t)h, killer));
    return 1;
}

static int l_romp_get_vehicle_health(lua_State *Ls) {
    int vid = (int)luaL_checkinteger(Ls, 1);
    Vehicle *v = vehicle_by_id(vid);
    if (!v) { lua_pushnil(Ls); return 1; }
    lua_pushinteger(Ls, v->health);
    return 1;
}

static int l_romp_damage_vehicle(lua_State *Ls) {
    int vid = (int)luaL_checkinteger(Ls, 1);
    lua_Integer amount = luaL_checkinteger(Ls, 2);
    int killer = (int)luaL_optinteger(Ls, 3, -1);
    if (amount < 0) amount = 0;
    if (amount > VEH_HEALTH_MAX) amount = VEH_HEALTH_MAX;
    int nh = vehicle_damage(vid, (uint16_t)amount, killer);
    if (nh < 0) { lua_pushnil(Ls); return 1; }
    lua_pushinteger(Ls, nh);
    return 1;
}

static int l_romp_put_player_in_vehicle(lua_State *Ls) {
    int pid = (int)luaL_checkinteger(Ls, 1);
    int vid = (int)luaL_checkinteger(Ls, 2);
    int seat = (int)luaL_checkinteger(Ls, 3);
    lua_pushboolean(Ls, vehicle_put_player(pid, vid, seat));
    return 1;
}

static int l_romp_remove_player_from_vehicle(lua_State *Ls) {
    int pid = (int)luaL_checkinteger(Ls, 1);
    lua_pushboolean(Ls, vehicle_remove_player(pid));
    return 1;
}

static int l_romp_is_player_in_vehicle(lua_State *Ls) {
    int pid = (int)luaL_checkinteger(Ls, 1);
    lua_pushboolean(Ls, vehicle_of_player(pid) >= 0);
    return 1;
}

static int l_romp_get_player_vehicle(lua_State *Ls) {
    int pid = (int)luaL_checkinteger(Ls, 1);
    int vid = vehicle_of_player(pid);
    if (vid < 0) { lua_pushnil(Ls); return 1; }
    lua_pushinteger(Ls, vid);
    return 1;
}

static int l_romp_get_player_vehicle_seat(lua_State *Ls) {
    int pid = (int)luaL_checkinteger(Ls, 1);
    int seat = vehicle_seat_of_player(pid);
    if (seat < 0) { lua_pushnil(Ls); return 1; }
    lua_pushinteger(Ls, seat);
    return 1;
}

static int l_romp_get_vehicle_model(lua_State *Ls) {
    int vid = (int)luaL_checkinteger(Ls, 1);
    Vehicle *v = vehicle_by_id(vid);
    if (!v) { lua_pushnil(Ls); return 1; }
    lua_pushinteger(Ls, v->model);
    return 1;
}

static int l_romp_get_vehicle_colors(lua_State *Ls) {
    int vid = (int)luaL_checkinteger(Ls, 1);
    Vehicle *v = vehicle_by_id(vid);
    if (!v) { lua_pushnil(Ls); return 1; }
    lua_pushinteger(Ls, v->c1);
    lua_pushinteger(Ls, v->c2);
    return 2;
}

static int l_romp_set_vehicle_colors(lua_State *Ls) {
    int vid = (int)luaL_checkinteger(Ls, 1);
    uint8_t c1 = (uint8_t)luaL_checkinteger(Ls, 2);
    uint8_t c2 = (uint8_t)luaL_checkinteger(Ls, 3);
    lua_pushboolean(Ls, vehicle_set_colors(vid, c1, c2));
    return 1;
}

static int l_romp_get_vehicle_driver(lua_State *Ls) {
    int vid = (int)luaL_checkinteger(Ls, 1);
    int pid = vehicle_occupant(vid, 0);
    if (pid < 0) { lua_pushnil(Ls); return 1; }
    lua_pushinteger(Ls, pid);
    return 1;
}

static int l_romp_get_vehicle_passenger(lua_State *Ls) {
    int vid = (int)luaL_checkinteger(Ls, 1);
    int seat = (int)luaL_checkinteger(Ls, 2);
    int pid = vehicle_occupant(vid, seat);
    if (pid < 0) { lua_pushnil(Ls); return 1; }
    lua_pushinteger(Ls, pid);
    return 1;
}

static int l_romp_respawn_vehicle(lua_State *Ls) {
    int vid = (int)luaL_checkinteger(Ls, 1);
    lua_pushboolean(Ls, vehicle_respawn(vid));
    return 1;
}

static int l_romp_get_vehicles(lua_State *Ls) {
    lua_newtable(Ls);
    int i = 1;
    for (Vehicle *v = vehicle_first(); v; v = vehicle_next(v)) {
        lua_newtable(Ls);
        lua_pushinteger(Ls, v->id);      lua_setfield(Ls, -2, "id");
        lua_pushinteger(Ls, v->model);   lua_setfield(Ls, -2, "model");
        lua_pushnumber(Ls, (lua_Number)v->x); lua_setfield(Ls, -2, "x");
        lua_pushnumber(Ls, (lua_Number)v->y); lua_setfield(Ls, -2, "y");
        lua_pushnumber(Ls, (lua_Number)v->z); lua_setfield(Ls, -2, "z");
        lua_pushnumber(Ls, (lua_Number)v->heading); lua_setfield(Ls, -2, "heading");
        lua_pushinteger(Ls, v->c1);      lua_setfield(Ls, -2, "c1");
        lua_pushinteger(Ls, v->c2);      lua_setfield(Ls, -2, "c2");
        lua_pushinteger(Ls, v->health);  lua_setfield(Ls, -2, "health");
        lua_pushboolean(Ls, v->destroyed); lua_setfield(Ls, -2, "destroyed");
        if (v->driver >= 0) { lua_pushinteger(Ls, v->driver); }
        else { lua_pushnil(Ls); }
        lua_setfield(Ls, -2, "driver");
        lua_newtable(Ls);
        for (int s = 0; s < VEH_SEATS; s++) {
            int occ = vehicle_occupant(v->id, s);
            if (occ >= 0) { lua_pushinteger(Ls, occ); }
            else { lua_pushnil(Ls); }
            lua_rawseti(Ls, -2, s + 1); /* seats[1]=sopir, [2..4]=penumpang */
        }
        lua_setfield(Ls, -2, "seats");
        lua_rawseti(Ls, -2, i++);
    }
    return 1;
}

/* ---------------- Senjata ala SA-MP ---------------- */

static int l_romp_give_weapon(lua_State *Ls) {
    int pid = (int)luaL_checkinteger(Ls, 1);
    int wid = (int)luaL_checkinteger(Ls, 2);
    int ammo = (int)luaL_checkinteger(Ls, 3);
    Client *c = client_by_id(pid);
    lua_pushboolean(Ls, c && weapon_give(c, wid, ammo));
    return 1;
}

static int l_romp_remove_player_weapon(lua_State *Ls) {
    int pid = (int)luaL_checkinteger(Ls, 1);
    int wid = (int)luaL_checkinteger(Ls, 2);
    Client *c = client_by_id(pid);
    int ok = 0;
    if (c) {
        int slot = weapon_slot(wid);
        if (slot >= 0 && c->wpn[slot] == (uint16_t)wid)
            ok = weapon_remove_slot(c, slot);
    }
    lua_pushboolean(Ls, ok);
    return 1;
}

static int l_romp_reset_player_weapons(lua_State *Ls) {
    int pid = (int)luaL_checkinteger(Ls, 1);
    Client *c = client_by_id(pid);
    if (c) weapon_reset(c);
    lua_pushboolean(Ls, c != NULL);
    return 1;
}

static int l_romp_get_player_weapon(lua_State *Ls) {
    int pid = (int)luaL_checkinteger(Ls, 1);
    Client *c = client_by_id(pid);
    if (!c) { lua_pushnil(Ls); return 1; }
    lua_pushinteger(Ls, c->wpn[c->wpn_slot]);
    lua_pushinteger(Ls, c->wpn_ammo[c->wpn_slot]);
    lua_pushinteger(Ls, c->wpn_slot);
    return 3;
}

static int l_romp_get_player_weapon_data(lua_State *Ls) {
    int pid = (int)luaL_checkinteger(Ls, 1);
    int slot = (int)luaL_checkinteger(Ls, 2);
    Client *c = client_by_id(pid);
    if (!c || slot < 0 || slot >= WPN_SLOTS) { lua_pushnil(Ls); return 1; }
    lua_pushinteger(Ls, c->wpn[slot]);
    lua_pushinteger(Ls, c->wpn_ammo[slot]);
    return 2;
}

static int l_romp_weapon_slot(lua_State *Ls) {
    int wid = (int)luaL_checkinteger(Ls, 1);
    lua_pushinteger(Ls, weapon_slot(wid));
    return 1;
}

/* ---------------- Interior & virtual world ala SA-MP ---------------- */

static int l_romp_get_player_interior(lua_State *Ls) {
    int id = (int)luaL_checkinteger(Ls, 1);
    Client *c = client_by_id(id);
    if (!c || !c->handshaked) { lua_pushnil(Ls); return 1; }
    lua_pushinteger(Ls, c->interior);
    return 1;
}

static int l_romp_set_player_interior(lua_State *Ls) {
    int id = (int)luaL_checkinteger(Ls, 1);
    int interior = (int)luaL_checkinteger(Ls, 2);
    Client *c = client_by_id(id);
    if (!c || !c->handshaked) { lua_pushboolean(Ls, 0); return 1; }
    player_set_world(c, interior, c->vw);
    lua_pushboolean(Ls, 1);
    return 1;
}

static int l_romp_get_player_vw(lua_State *Ls) {
    int id = (int)luaL_checkinteger(Ls, 1);
    Client *c = client_by_id(id);
    if (!c || !c->handshaked) { lua_pushnil(Ls); return 1; }
    lua_pushinteger(Ls, (lua_Integer)c->vw);
    return 1;
}

static int l_romp_set_player_vw(lua_State *Ls) {
    int id = (int)luaL_checkinteger(Ls, 1);
    lua_Integer vw = luaL_checkinteger(Ls, 2);
    Client *c = client_by_id(id);
    if (!c || !c->handshaked) { lua_pushboolean(Ls, 0); return 1; }
    if (vw < 0) vw = 0;
    if (vw > 0x7FFFFFFF) vw = 0x7FFFFFFF;
    player_set_world(c, c->interior, (uint32_t)vw);
    lua_pushboolean(Ls, 1);
    return 1;
}

static int l_romp_get_vehicle_vw(lua_State *Ls) {
    int id = (int)luaL_checkinteger(Ls, 1);
    Vehicle *v = vehicle_by_id(id);
    if (!v) { lua_pushnil(Ls); return 1; }
    lua_pushinteger(Ls, (lua_Integer)v->vw);
    return 1;
}

static int l_romp_set_vehicle_vw(lua_State *Ls) {
    int id = (int)luaL_checkinteger(Ls, 1);
    lua_Integer vw = luaL_checkinteger(Ls, 2);
    if (vw < 0) vw = 0;
    if (vw > 0x7FFFFFFF) vw = 0x7FFFFFFF;
    lua_pushboolean(Ls, vehicle_set_vw(id, (uint32_t)vw));
    return 1;
}

/* ---------------- API Filterscript ala SA-MP ----------------
 * romp.loadFilterScript(name) -> bool   (true = dimuat / sudah dimuat)
 * romp.unloadFilterScript(name) -> bool (true = berhasil di-unload)
 * romp.getFilterScripts() -> tabel array berisi nama filterscript aktif */
static int l_romp_load_fs(lua_State *Ls) {
    const char *name = luaL_checkstring(Ls, 1);
    int r = fs_load(name);
    lua_pushboolean(Ls, r == 0 || r == 1);
    return 1;
}

static int l_romp_unload_fs(lua_State *Ls) {
    const char *name = luaL_checkstring(Ls, 1);
    lua_pushboolean(Ls, fs_unload(name) == 0);
    return 1;
}

static int l_romp_get_fs(lua_State *Ls) {
    lua_newtable(Ls);
    int n = fs_count();
    for (int k = 0; k < n; k++) {
        lua_pushstring(Ls, fs_get_name(k));
        lua_rawseti(Ls, -2, k + 1);
    }
    return 1;
}

/* ---------------- API NPC/bot ala SA-MP (ConnectNPC) ----------------
 * romp.connectNPC(name) -> npcid | nil, err
 * romp.disconnectNPC(npcid[, reason]) -> bool
 * romp.isNPC(playerid) -> bool
 * romp.setNPCPos(npcid, x, y, z) -> bool (relay 0x11 ke pemain yang melihat)
 * romp.npcSendChat(npcid, text) -> bool (picu onPlayerChat, lalu broadcast)
 * romp.getNPCs() -> tabel array berisi id NPC yang online */
static int l_romp_connect_npc(lua_State *Ls) {
    const char *name = luaL_checkstring(Ls, 1);
    int id = npc_connect(name);
    if (id < 0) {
        lua_pushnil(Ls);
        lua_pushstring(Ls, "nama invalid / sudah dipakai / server penuh");
        return 2;
    }
    lua_pushinteger(Ls, id);
    return 1;
}

static int l_romp_disconnect_npc(lua_State *Ls) {
    int id = (int)luaL_checkinteger(Ls, 1);
    int reason = (int)luaL_optinteger(Ls, 2, 0);
    Client *c = client_by_id(id);
    if (!c || !c->is_npc) { lua_pushboolean(Ls, 0); return 1; }
    npc_disconnect(id, reason);
    lua_pushboolean(Ls, 1);
    return 1;
}

static int l_romp_is_npc(lua_State *Ls) {
    int id = (int)luaL_checkinteger(Ls, 1);
    Client *c = client_by_id(id);
    lua_pushboolean(Ls, c && c->is_npc);
    return 1;
}

static int l_romp_set_npc_pos(lua_State *Ls) {
    int id = (int)luaL_checkinteger(Ls, 1);
    float x = (float)luaL_checknumber(Ls, 2);
    float y = (float)luaL_checknumber(Ls, 3);
    float z = (float)luaL_checknumber(Ls, 4);
    lua_pushboolean(Ls, npc_set_pos(id, x, y, z));
    return 1;
}

static int l_romp_npc_send_chat(lua_State *Ls) {
    int id = (int)luaL_checkinteger(Ls, 1);
    const char *text = luaL_checkstring(Ls, 2);
    lua_pushboolean(Ls, npc_send_chat(id, text));
    return 1;
}

static int l_romp_get_npcs(lua_State *Ls) {
    lua_newtable(Ls);
    int i = 1;
    for (Client *c = g_clients; c; c = c->next) {
        if (!c->handshaked || c->dead || !c->is_npc) continue;
        lua_pushinteger(Ls, c->id);
        lua_rawseti(Ls, -2, i++);
    }
    return 1;
}

/* ---------------- Init ---------------- */

/* Dipakai script_setup sebelum definisinya (di bawah). */
static void call_event_noret(const char *ev);
static void call_event_noret_s(lua_State *Ls, const char *ev);

/* Ambil folder dari path script ("a/b/c.lua" -> "a/b"). */
static void script_dir_of(const char *path, char *out, size_t out_sz) {
    const char *slash = strrchr(path, '/');
#ifdef _WIN32
    const char *bs = strrchr(path, '\\');
    if (bs && (!slash || bs > slash)) slash = bs;
#endif
    size_t dl = slash ? (size_t)(slash - path) : 0;
    if (dl > out_sz - 1) dl = out_sz - 1;
    if (dl == 0) { out[0] = '.'; out[1] = '\0'; return; }
    memcpy(out, path, dl);
    out[dl] = '\0';
}

/* Daftarkan tabel romp.* + tabel registry NUI callback ke satu state Lua. */
static void script_register_api(lua_State *Ls) {
    /* tabel romp */
    lua_newtable(Ls);
    lua_pushcfunction(Ls, l_romp_log);              lua_setfield(Ls, -2, "log");
    lua_pushcfunction(Ls, l_romp_player_count);     lua_setfield(Ls, -2, "playerCount");
    lua_pushcfunction(Ls, l_romp_get_player_name);  lua_setfield(Ls, -2, "getPlayerName");
    lua_pushcfunction(Ls, l_romp_get_players);      lua_setfield(Ls, -2, "getPlayers");
    lua_pushcfunction(Ls, l_romp_get_player_pos);  lua_setfield(Ls, -2, "getPlayerPos");
    lua_pushcfunction(Ls, l_romp_send_chat_to_all); lua_setfield(Ls, -2, "sendChatToAll");
    lua_pushcfunction(Ls, l_romp_send_chat_to_player); lua_setfield(Ls, -2, "sendChatToPlayer");
    lua_pushcfunction(Ls, l_romp_kick_player);      lua_setfield(Ls, -2, "kickPlayer");
    lua_pushcfunction(Ls, l_romp_get_tick);         lua_setfield(Ls, -2, "getTick");
    lua_pushcfunction(Ls, l_romp_get_config);       lua_setfield(Ls, -2, "getConfig");
    lua_pushcfunction(Ls, l_romp_send_nui_message); lua_setfield(Ls, -2, "sendNUIMessage");
    lua_pushcfunction(Ls, l_romp_on_nui_callback);  lua_setfield(Ls, -2, "onNUICallback");
    lua_pushcfunction(Ls, l_romp_set_nui_focus);    lua_setfield(Ls, -2, "setNUIFocus");
    lua_pushcfunction(Ls, l_romp_show_dialog);      lua_setfield(Ls, -2, "showDialog");
    lua_pushcfunction(Ls, l_romp_hide_dialog);      lua_setfield(Ls, -2, "hideDialog");
    lua_pushcfunction(Ls, l_romp_get_player_ip);   lua_setfield(Ls, -2, "getPlayerIP");
    lua_pushcfunction(Ls, l_romp_set_hostname);    lua_setfield(Ls, -2, "setHostname");
    lua_pushcfunction(Ls, l_romp_set_motd);        lua_setfield(Ls, -2, "setMOTD");
    lua_pushcfunction(Ls, l_romp_set_password);    lua_setfield(Ls, -2, "setPassword");
    lua_pushcfunction(Ls, l_romp_get_server_info); lua_setfield(Ls, -2, "getServerInfo");
    lua_pushcfunction(Ls, l_romp_ban_ip);          lua_setfield(Ls, -2, "banIP");
    lua_pushcfunction(Ls, l_romp_unban_ip);        lua_setfield(Ls, -2, "unbanIP");
    lua_pushcfunction(Ls, l_romp_is_ip_banned);    lua_setfield(Ls, -2, "isIPBanned");
    lua_pushcfunction(Ls, l_romp_announce_now);    lua_setfield(Ls, -2, "announceNow");
    /* API kendaraan ala SA-MP */
    lua_pushcfunction(Ls, l_romp_create_vehicle);   lua_setfield(Ls, -2, "createVehicle");
    lua_pushcfunction(Ls, l_romp_destroy_vehicle);  lua_setfield(Ls, -2, "destroyVehicle");
    lua_pushcfunction(Ls, l_romp_set_vehicle_pos);  lua_setfield(Ls, -2, "setVehiclePos");
    lua_pushcfunction(Ls, l_romp_get_vehicle_pos);  lua_setfield(Ls, -2, "getVehiclePos");
    lua_pushcfunction(Ls, l_romp_set_vehicle_health); lua_setfield(Ls, -2, "setVehicleHealth");
    lua_pushcfunction(Ls, l_romp_get_vehicle_health); lua_setfield(Ls, -2, "getVehicleHealth");
    lua_pushcfunction(Ls, l_romp_damage_vehicle);   lua_setfield(Ls, -2, "damageVehicle");
    lua_pushcfunction(Ls, l_romp_put_player_in_vehicle); lua_setfield(Ls, -2, "putPlayerInVehicle");
    lua_pushcfunction(Ls, l_romp_remove_player_from_vehicle); lua_setfield(Ls, -2, "removePlayerFromVehicle");
    lua_pushcfunction(Ls, l_romp_is_player_in_vehicle); lua_setfield(Ls, -2, "isPlayerInVehicle");
    lua_pushcfunction(Ls, l_romp_get_player_vehicle); lua_setfield(Ls, -2, "getPlayerVehicle");
    lua_pushcfunction(Ls, l_romp_get_player_vehicle_seat); lua_setfield(Ls, -2, "getPlayerVehicleSeat");
    lua_pushcfunction(Ls, l_romp_get_vehicle_model); lua_setfield(Ls, -2, "getVehicleModel");
    lua_pushcfunction(Ls, l_romp_get_vehicle_colors); lua_setfield(Ls, -2, "getVehicleColors");
    lua_pushcfunction(Ls, l_romp_set_vehicle_colors); lua_setfield(Ls, -2, "setVehicleColors");
    lua_pushcfunction(Ls, l_romp_get_vehicle_driver); lua_setfield(Ls, -2, "getVehicleDriver");
    lua_pushcfunction(Ls, l_romp_get_vehicle_passenger); lua_setfield(Ls, -2, "getVehiclePassenger");
    lua_pushcfunction(Ls, l_romp_respawn_vehicle);  lua_setfield(Ls, -2, "respawnVehicle");
    lua_pushcfunction(Ls, l_romp_get_vehicles);      lua_setfield(Ls, -2, "getVehicles");
    /* senjata ala SA-MP */
    lua_pushcfunction(Ls, l_romp_give_weapon);       lua_setfield(Ls, -2, "giveWeapon");
    lua_pushcfunction(Ls, l_romp_remove_player_weapon); lua_setfield(Ls, -2, "removePlayerWeapon");
    lua_pushcfunction(Ls, l_romp_reset_player_weapons); lua_setfield(Ls, -2, "resetPlayerWeapons");
    lua_pushcfunction(Ls, l_romp_get_player_weapon);  lua_setfield(Ls, -2, "getPlayerWeapon");
    lua_pushcfunction(Ls, l_romp_get_player_weapon_data); lua_setfield(Ls, -2, "getPlayerWeaponData");
    lua_pushcfunction(Ls, l_romp_weapon_slot);        lua_setfield(Ls, -2, "weaponSlot");
    /* interior & virtual world ala SA-MP */
    lua_pushcfunction(Ls, l_romp_get_player_interior); lua_setfield(Ls, -2, "getPlayerInterior");
    lua_pushcfunction(Ls, l_romp_set_player_interior); lua_setfield(Ls, -2, "setPlayerInterior");
    lua_pushcfunction(Ls, l_romp_get_player_vw); lua_setfield(Ls, -2, "getPlayerVirtualWorld");
    lua_pushcfunction(Ls, l_romp_set_player_vw); lua_setfield(Ls, -2, "setPlayerVirtualWorld");
    lua_pushcfunction(Ls, l_romp_get_vehicle_vw); lua_setfield(Ls, -2, "getVehicleVirtualWorld");
    lua_pushcfunction(Ls, l_romp_set_vehicle_vw); lua_setfield(Ls, -2, "setVehicleVirtualWorld");
    /* filterscript ala SA-MP */
    lua_pushcfunction(Ls, l_romp_load_fs);        lua_setfield(Ls, -2, "loadFilterScript");
    lua_pushcfunction(Ls, l_romp_unload_fs);      lua_setfield(Ls, -2, "unloadFilterScript");
    lua_pushcfunction(Ls, l_romp_get_fs);         lua_setfield(Ls, -2, "getFilterScripts");
    /* NPC/bot ala SA-MP (ConnectNPC) */
    lua_pushcfunction(Ls, l_romp_connect_npc);    lua_setfield(Ls, -2, "connectNPC");
    lua_pushcfunction(Ls, l_romp_disconnect_npc); lua_setfield(Ls, -2, "disconnectNPC");
    lua_pushcfunction(Ls, l_romp_is_npc);         lua_setfield(Ls, -2, "isNPC");
    lua_pushcfunction(Ls, l_romp_set_npc_pos);    lua_setfield(Ls, -2, "setNPCPos");
    lua_pushcfunction(Ls, l_romp_npc_send_chat);  lua_setfield(Ls, -2, "npcSendChat");
    lua_pushcfunction(Ls, l_romp_get_npcs);       lua_setfield(Ls, -2, "getNPCs");
    /* konstanta hittype tembakan ala SA-MP */
    lua_pushinteger(Ls, 0); lua_setfield(Ls, -2, "WPN_HIT_NONE");
    lua_pushinteger(Ls, 1); lua_setfield(Ls, -2, "WPN_HIT_PLAYER");
    lua_pushinteger(Ls, 2); lua_setfield(Ls, -2, "WPN_HIT_VEHICLE");
    lua_pushinteger(Ls, 3); lua_setfield(Ls, -2, "WPN_HIT_OBJECT");
    /* konstanta kind chat */
    lua_pushinteger(Ls, KIND_CHAT); lua_setfield(Ls, -2, "KIND_CHAT");
    lua_pushinteger(Ls, KIND_INFO); lua_setfield(Ls, -2, "KIND_INFO");
    lua_pushinteger(Ls, KIND_ME);   lua_setfield(Ls, -2, "KIND_ME");
    lua_pushinteger(Ls, KIND_PM);   lua_setfield(Ls, -2, "KIND_PM");
    lua_pushinteger(Ls, KIND_SYS);  lua_setfield(Ls, -2, "KIND_SYS");
    /* konstanta style dialog ala SA-MP */
    lua_pushinteger(Ls, 0); lua_setfield(Ls, -2, "DIALOG_MSGBOX");
    lua_pushinteger(Ls, 1); lua_setfield(Ls, -2, "DIALOG_INPUT");
    lua_pushinteger(Ls, 2); lua_setfield(Ls, -2, "DIALOG_LIST");
    lua_pushinteger(Ls, 3); lua_setfield(Ls, -2, "DIALOG_PASSWORD");
    lua_setglobal(Ls, "romp");

    /* tabel registry untuk NUI callback — disimpan per-state */
    lua_newtable(Ls);
    int ref = luaL_ref(Ls, LUA_REGISTRYINDEX);
    lua_pushinteger(Ls, ref);
    lua_setfield(Ls, LUA_REGISTRYINDEX, "romp.nui_cb_ref");
}

/* Muat helper Lua (dialog.lua, textdraw.lua, ...) untuk satu modul script.
 * Dicari di folder modul dulu, lalu fallback ke folder gamemode. */
static void script_load_helper(Script *s, const char *dir,
                               const char *fallback_dir, const char *file,
                               const char *desc) {
    char path[600];
    snprintf(path, sizeof path, "%s/%s", dir, file);
    FILE *f = fopen(path, "r");
    if (!f && fallback_dir && fallback_dir[0] && strcmp(fallback_dir, dir) != 0) {
        snprintf(path, sizeof path, "%s/%s", fallback_dir, file);
        f = fopen(path, "r");
    }
    if (!f) {
        log_msg("[lua] helper %s tidak ditemukan untuk '%s', dilewati", file, s->name);
        return;
    }
    fclose(f);
    if (luaL_loadfile(s->L, path) != LUA_OK ||
        lua_pcall(s->L, 0, 0, 0) != LUA_OK) {
        log_msg("[lua] %s gagal dimuat untuk '%s' (%s): %s", file, s->name, path,
                lua_tostring(s->L, -1) ? lua_tostring(s->L, -1) : "?");
        lua_pop(s->L, 1);
    } else {
        log_msg("[lua] %s dimuat untuk '%s' (%s)", file, s->name, desc);
    }
}

static const struct { const char *file; const char *desc; } g_helpers[] = {
    { "dialog.lua",     "event onDialogResponse aktif" },
    { "textdraw.lua",   "API textdraw aktif" },
    { "checkpoint.lua", "API checkpoint aktif" },
    { "pickup.lua",     "API pickup aktif" },
    { "mapicon.lua",    "API map icon aktif" },
    { "vehicle.lua",    "API kendaraan aktif" },
};

/* Siapkan satu modul script: state baru + API + file + helper.
 * fallback_dir = folder gamemode (untuk helper), NULL kalau modul ini gamemode. */
static int script_setup(Script *s, const char *path, const char *name,
                        const char *fallback_dir) {
    memset(s, 0, sizeof *s);
    s->L = luaL_newstate();
    if (!s->L) return -1;
    luaL_openlibs(s->L);
    script_register_api(s->L);
    strncpy(s->name, name, sizeof s->name - 1);

    if (luaL_loadfile(s->L, path) != LUA_OK ||
        lua_pcall(s->L, 0, 0, 0) != LUA_OK) {
        log_msg("[lua] gagal load '%s': %s", path,
                lua_tostring(s->L, -1) ? lua_tostring(s->L, -1) : "?");
        lua_close(s->L);
        s->L = NULL;
        return -1;
    }
    char dir[512];
    script_dir_of(path, dir, sizeof dir);
    for (size_t i = 0; i < sizeof g_helpers / sizeof g_helpers[0]; i++)
        script_load_helper(s, dir, fallback_dir,
                           g_helpers[i].file, g_helpers[i].desc);
    return 0;
}

/* Close state yang ditunda (dipanggil di luar event: onTick / shutdown). */
static void script_flush_pending(void) {
    for (int i = 0; i < g_pending_n; i++)
        if (g_pending_close[i]) lua_close(g_pending_close[i]);
    g_pending_n = 0;
}

static void script_teardown(Script *s) {
    if (s->L) { lua_close(s->L); s->L = NULL; }
    s->active = 0;
}

/* ---------------- Filterscript ala SA-MP ---------------- */

static Script *fs_find(const char *name) {
    for (int i = 0; i < MAX_FILTERSCRIPTS; i++)
        if (g_fs[i].active && !strcmp(g_fs[i].name, name)) return &g_fs[i];
    return NULL;
}

/* 0 = dimuat, 1 = sudah dimuat, -1 = slot penuh, -2 = file tidak ada/gagal */
int fs_load(const char *name) {
    if (!name || !name[0] || strlen(name) > 48) return -2;
    /* cegah path traversal: nama polos saja, file = <fs_dir>/<name>.lua */
    if (strchr(name, '/') || strchr(name, '\\') || strstr(name, "..")) return -2;
    if (!g_fs_dir[0]) return -2; /* lua_init belum jalan */
    if (fs_find(name)) return 1;
    Script *s = NULL;
    for (int i = 0; i < MAX_FILTERSCRIPTS; i++)
        if (!g_fs[i].active) { s = &g_fs[i]; break; }
    if (!s) {
        log_msg("[lua] filterscript '%s': slot penuh (maks %d)", name, MAX_FILTERSCRIPTS);
        return -1;
    }
    char path[600];
    snprintf(path, sizeof path, "%s/%s.lua", g_fs_dir, name);
    if (script_setup(s, path, name, g_gm_dir) != 0) return -2;
    s->active = 1;
    log_msg("[lua] filterscript '%s' dimuat", name);
    call_event_noret_s(s->L, "onFilterScriptInit");
    return 0;
}

int fs_unload(const char *name) {
    Script *s = fs_find(name);
    if (!s) return -1;
    call_event_noret_s(s->L, "onFilterScriptExit");
    /* Tunda lua_close: unload bisa dipicu dari dalam event modul itu sendiri. */
    if (g_pending_n < MAX_FILTERSCRIPTS)
        g_pending_close[g_pending_n++] = s->L;
    else
        lua_close(s->L);
    s->L = NULL;
    s->active = 0;
    log_msg("[lua] filterscript '%s' di-unload", name);
    return 0;
}

int fs_reload(const char *name) {
    if (fs_find(name)) fs_unload(name);
    return fs_load(name);
}

/* Muat daftar "fs1 fs2 fs3" dari server.cfg. */
void fs_load_configured(const char *list) {
    if (!list || !list[0]) return;
    char buf[512];
    strncpy(buf, list, sizeof buf - 1);
    buf[sizeof buf - 1] = '\0';
    for (char *tok = strtok(buf, " \t"); tok; tok = strtok(NULL, " \t")) {
        int r = fs_load(tok);
        if (r == -2)
            log_msg("[lua] filterscript '%s' gagal: file %s/%s.lua tidak ditemukan",
                    tok, g_fs_dir, tok);
    }
}

int fs_count(void) {
    int n = 0;
    for (int i = 0; i < MAX_FILTERSCRIPTS; i++)
        if (g_fs[i].active) n++;
    return n;
}

const char *fs_get_name(int idx) {
    int n = 0;
    for (int i = 0; i < MAX_FILTERSCRIPTS; i++)
        if (g_fs[i].active) { if (n++ == idx) return g_fs[i].name; }
    return NULL;
}

int lua_init(const char *script_path) {
    if (script_setup(&g_gm, script_path, "gamemode", NULL) != 0) return -1;
    g_gm.active = 1;
    script_dir_of(script_path, g_gm_dir, sizeof g_gm_dir);
    g_gm_dir[sizeof g_gm_dir - 15] = '\0'; /* sisakan ruang untuk "/filterscripts" */
    snprintf(g_fs_dir, sizeof g_fs_dir, "%s/filterscripts", g_gm_dir);
    log_msg("[lua] gamemode dimuat: %s", script_path);
    log_msg("[lua] folder filterscripts: %s", g_fs_dir);
    /* event init: onGamemodeInit() */
    call_event_noret("onGamemodeInit");
    return 0;
}

void lua_shutdown(void) {
    for (int i = 0; i < MAX_FILTERSCRIPTS; i++)
        if (g_fs[i].active) script_teardown(&g_fs[i]);
    script_teardown(&g_gm);
    script_flush_pending();
}

/* ---------------- Dispatch event ---------------- */

/* Panggil fungsi global Lua `ev` di state Ls. push_args mendorong argumen &
 * mengembalikan jumlahnya. Kembalikan 1 jika fungsi ada & return true. */
typedef int (*push_args_fn)(lua_State *Ls, void *ud);

static int call_event_s(lua_State *Ls, const char *ev, push_args_fn push, void *ud) {
    lua_getglobal(Ls, ev);
    if (!lua_isfunction(Ls, -1)) { lua_pop(Ls, 1); return 0; }
    int nargs = push(Ls, ud);
    if (lua_pcall(Ls, nargs, 1, 0) != LUA_OK) {
        log_msg("[lua] error di %s: %s", ev,
                lua_tostring(Ls, -1) ? lua_tostring(Ls, -1) : "?");
        lua_pop(Ls, 1);
        return 0;
    }
    int handled = lua_toboolean(Ls, -1);
    lua_pop(Ls, 1);
    return handled;
}

static int call_event(const char *ev, push_args_fn push, void *ud) {
    if (!g_gm.L) return 0;
    return call_event_s(g_gm.L, ev, push, ud);
}

static void call_event_noret_s(lua_State *Ls, const char *ev) {
    lua_getglobal(Ls, ev);
    if (!lua_isfunction(Ls, -1)) { lua_pop(Ls, 1); return; }
    if (lua_pcall(Ls, 0, 0, 0) != LUA_OK) {
        log_msg("[lua] error di %s: %s", ev,
                lua_tostring(Ls, -1) ? lua_tostring(Ls, -1) : "?");
        lua_pop(Ls, 1);
    }
}

static void call_event_noret(const char *ev) {
    if (g_gm.L) call_event_noret_s(g_gm.L, ev);
}

/* Dispatch event ke semua filterscript aktif (urutan load). Ala SA-MP:
 * filterscript dipanggil DULU, baru gamemode; true dari mana pun = handled. */
static int fs_event(const char *ev, push_args_fn push, void *ud) {
    int handled = 0;
    for (int i = 0; i < MAX_FILTERSCRIPTS; i++)
        if (g_fs[i].active && g_fs[i].L)
            if (call_event_s(g_fs[i].L, ev, push, ud)) handled = 1;
    return handled;
}

static void fs_event_noret(const char *ev) {
    for (int i = 0; i < MAX_FILTERSCRIPTS; i++)
        if (g_fs[i].active && g_fs[i].L) call_event_noret_s(g_fs[i].L, ev);
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
    if (fs_event("onPlayerJoin", push_join, &e)) return 1;
    return call_event("onPlayerJoin", push_join, &e);
}

int lua_on_player_leave(int id, const char *name, int reason) {
    EvLeave e = { id, name, reason };
    if (fs_event("onPlayerLeave", push_leave, &e)) return 1;
    return call_event("onPlayerLeave", push_leave, &e);
}

int lua_on_player_chat(int id, const char *name, const char *text) {
    EvChat e = { id, name, text };
    if (fs_event("onPlayerChat", push_chat, &e)) return 1;
    return call_event("onPlayerChat", push_chat, &e);
}

int lua_on_player_command(int id, const char *name, const char *cmd, const char *args) {
    EvCmd e = { id, name, cmd, args };
    if (fs_event("onPlayerCommand", push_cmd, &e)) return 1;
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
    if (fs_event("onConsoleCommand", push_con, &e)) return 1;
    return call_event("onConsoleCommand", push_con, &e);
}

/* onPlayerRequestConnect(name, ip) -> true, "alasan" = tolak koneksi. */
static int player_request_s(Script *s, const char *name, const char *ip,
                            char *reason_out, size_t reason_sz) {
    lua_State *Ls = s->L;
    if (!Ls) return 0;
    lua_getglobal(Ls, "onPlayerRequestConnect");
    if (!lua_isfunction(Ls, -1)) { lua_pop(Ls, 1); return 0; }
    lua_pushstring(Ls, name);
    lua_pushstring(Ls, ip);
    if (lua_pcall(Ls, 2, 2, 0) != LUA_OK) {
        log_msg("[lua] error di onPlayerRequestConnect (%s): %s", s->name,
                lua_tostring(Ls, -1) ? lua_tostring(Ls, -1) : "?");
        lua_pop(Ls, 1);
        return 0;
    }
    int reject = lua_toboolean(Ls, -2);
    if (reject && lua_isstring(Ls, -1) && reason_out && reason_sz > 0) {
        strncpy(reason_out, lua_tostring(Ls, -1), reason_sz - 1);
        reason_out[reason_sz - 1] = '\0';
    }
    lua_pop(Ls, 2);
    return reject;
}
int lua_on_player_request(const char *name, const char *ip,
                          char *reason_out, size_t reason_sz) {
    for (int i = 0; i < MAX_FILTERSCRIPTS; i++)
        if (g_fs[i].active &&
            player_request_s(&g_fs[i], name, ip, reason_out, reason_sz))
            return 1;
    return player_request_s(&g_gm, name, ip, reason_out, reason_sz);
}

/* ---------------- Event kendaraan ala SA-MP ---------------- */

typedef struct { int pid, vid, seat; } EvVehSeat;
static int push_veh_seat(lua_State *Ls, void *ud) {
    EvVehSeat *e = (EvVehSeat *)ud;
    lua_pushinteger(Ls, e->pid);
    lua_pushinteger(Ls, e->vid);
    lua_pushinteger(Ls, e->seat);
    return 3;
}

typedef struct { int pid, vid; } EvVehPidVid;
static int push_veh_pidvid(lua_State *Ls, void *ud) {
    EvVehPidVid *e = (EvVehPidVid *)ud;
    lua_pushinteger(Ls, e->pid);
    lua_pushinteger(Ls, e->vid);
    return 2;
}

typedef struct { int vid, a, b; } EvVehVAB;
static int push_veh_vab(lua_State *Ls, void *ud) {
    EvVehVAB *e = (EvVehVAB *)ud;
    lua_pushinteger(Ls, e->vid);
    lua_pushinteger(Ls, e->a);
    lua_pushinteger(Ls, e->b);
    return 3;
}

typedef struct { int vid, killer; } EvVehDeath;
static int push_veh_death(lua_State *Ls, void *ud) {
    EvVehDeath *e = (EvVehDeath *)ud;
    lua_pushinteger(Ls, e->vid);
    lua_pushinteger(Ls, e->killer);
    return 2;
}

static int push_veh_vid(lua_State *Ls, void *ud) {
    lua_pushinteger(Ls, *(int *)ud);
    return 1;
}

void lua_on_player_enter_vehicle(int pid, int vid, int seat) {
    EvVehSeat e = { pid, vid, seat };
    fs_event("onPlayerEnterVehicle", push_veh_seat, &e);
    call_event("onPlayerEnterVehicle", push_veh_seat, &e);
}

void lua_on_player_exit_vehicle(int pid, int vid) {
    EvVehPidVid e = { pid, vid };
    fs_event("onPlayerExitVehicle", push_veh_pidvid, &e);
    call_event("onPlayerExitVehicle", push_veh_pidvid, &e);
}

void lua_on_vehicle_damage(int vid, int damagerid, int newhealth) {
    EvVehVAB e = { vid, damagerid, newhealth };
    fs_event("onVehicleDamage", push_veh_vab, &e);
    call_event("onVehicleDamage", push_veh_vab, &e);
}

void lua_on_vehicle_death(int vid, int killerid) {
    EvVehDeath e = { vid, killerid };
    fs_event("onVehicleDeath", push_veh_death, &e);
    call_event("onVehicleDeath", push_veh_death, &e);
}

void lua_on_vehicle_spawn(int vid) {
    fs_event("onVehicleSpawn", push_veh_vid, &vid);
    call_event("onVehicleSpawn", push_veh_vid, &vid);
}

/* ---------------- Event senjata ala SA-MP ---------------- */

typedef struct { int pid, wid, ht, hid; float x, y, z; } EvShot;
static int push_shot(lua_State *Ls, void *ud) {
    EvShot *e = (EvShot *)ud;
    lua_pushinteger(Ls, e->pid);
    lua_pushinteger(Ls, e->wid);
    lua_pushinteger(Ls, e->ht);
    lua_pushinteger(Ls, e->hid);
    lua_pushnumber(Ls, (lua_Number)e->x);
    lua_pushnumber(Ls, (lua_Number)e->y);
    lua_pushnumber(Ls, (lua_Number)e->z);
    return 7;
}

void lua_on_player_weapon_shot(int pid, int weaponid, int hittype, int hitid,
                               float x, float y, float z) {
    EvShot e = { pid, weaponid, hittype, hitid, x, y, z };
    fs_event("onPlayerWeaponShot", push_shot, &e);
    call_event("onPlayerWeaponShot", push_shot, &e);
}

typedef struct { int pid; int oldi; int newi; } EvInt;
static int push_int(lua_State *Ls, void *ud) {
    EvInt *e = (EvInt*)ud;
    lua_pushinteger(Ls, e->pid);
    lua_pushinteger(Ls, e->oldi);
    lua_pushinteger(Ls, e->newi);
    return 3;
}

void lua_on_player_interior_change(int pid, int old_int, int new_int) {
    EvInt e = { pid, old_int, new_int };
    fs_event("onPlayerInteriorChange", push_int, &e);
    call_event("onPlayerInteriorChange", push_int, &e);
}

typedef struct { int pid; uint32_t oldv; uint32_t newv; } EvVw;
static int push_vw(lua_State *Ls, void *ud) {
    EvVw *e = (EvVw*)ud;
    lua_pushinteger(Ls, e->pid);
    lua_pushinteger(Ls, (lua_Integer)e->oldv);
    lua_pushinteger(Ls, (lua_Integer)e->newv);
    return 3;
}

void lua_on_player_vw_change(int pid, uint32_t old_vw, uint32_t new_vw) {
    EvVw e = { pid, old_vw, new_vw };
    fs_event("onPlayerVirtualWorldChange", push_vw, &e);
    call_event("onPlayerVirtualWorldChange", push_vw, &e);
}

/* Dispatch NUI callback ke handler terdaftar di satu modul script. */
static int nui_dispatch_s(Script *s, int id, const char *name, const char *data) {
    lua_State *Ls = s->L;
    if (!Ls) return 0;
    lua_getfield(Ls, LUA_REGISTRYINDEX, "romp.nui_cb_ref");
    int ref = (int)lua_tointeger(Ls, -1);
    lua_pop(Ls, 1);
    lua_rawgeti(Ls, LUA_REGISTRYINDEX, ref);
    lua_getfield(Ls, -1, name);
    lua_remove(Ls, -2); /* sisakan fn */
    if (!lua_isfunction(Ls, -1)) { lua_pop(Ls, 1); return 0; }
    lua_pushinteger(Ls, id);
    lua_pushstring(Ls, name);
    lua_pushstring(Ls, data);
    if (lua_pcall(Ls, 3, 0, 0) != LUA_OK) {
        log_msg("[lua] error di nui callback '%s' (%s): %s", name, s->name,
                lua_tostring(Ls, -1) ? lua_tostring(Ls, -1) : "?");
        lua_pop(Ls, 1);
    }
    return 1;
}

/* Dispatch NUI callback dari client ke handler Lua terdaftar.
 * Ala SA-MP: semua modul yang mendaftarkan nama callback itu dipanggil
 * (filterscript dulu, lalu gamemode) — mirip OnDialogResponse yang
 * dipanggil di tiap script dan tiap script cek id-nya sendiri. */
int lua_on_nui_callback(int id, const char *name, const char *data) {
    int handled = 0;
    for (int i = 0; i < MAX_FILTERSCRIPTS; i++)
        if (g_fs[i].active && nui_dispatch_s(&g_fs[i], id, name, data))
            handled = 1;
    if (nui_dispatch_s(&g_gm, id, name, data)) handled = 1;
    return handled;
}

void lua_on_tick(void) {
    fs_event_noret("onTick");
    call_event_noret("onTick");
    script_flush_pending();
}
