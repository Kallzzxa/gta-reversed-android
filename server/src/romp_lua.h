/*
 * RO-MP scripting layer — gamemode pakai Lua (ala FiveM).
 *
 * Server core (romp_server.c) memanggil fungsi lua_on_* di bawah saat event
 * terjadi. Kalau gamemode Lua mengembalikan true (= handled), penanganan
 * default di C dilewati; kalau false/nil/tidak ada fungsinya, C pakai default.
 */
#pragma once

#include <stddef.h> /* size_t untuk lua_on_player_request */
#include <stdint.h> /* uint32_t untuk event virtual world */

/* Satu modul script: gamemode atau satu filterscript. Tiap modul punya
 * lua_State sendiri (scope global terpisah ala SA-MP) tapi API romp.*
 * yang sama. */
#define MAX_FILTERSCRIPTS 16
typedef struct Script {
    struct lua_State *L;
    char name[64];
    int active;
} Script;

/* Muat & jalankan file gamemode. 0 = sukses, -1 = gagal (server tetap jalan tanpa Lua). */
int  lua_init(const char *script_path);
void lua_shutdown(void);
int  lua_active(void);

/* ---------------- Filterscript ala SA-MP ----------------
 * Tiap filterscript = modul Lua terpisah di <folder gamemode>/filterscripts/
 * <nama>.lua, dengan state Lua sendiri. Event dikirim ke filterscript DULU
 * (urutan load), baru gamemode; kalau ada yang return true (= handled),
 * gamemode dilewati. Lifecycle per modul: onFilterScriptInit() saat load,
 * onFilterScriptExit() saat unload. */
int  fs_load(const char *name);   /* 0 ok, 1 sudah dimuat, -1 slot penuh, -2 file/gagal */
int  fs_unload(const char *name); /* 0 ok, -1 tidak ditemukan */
int  fs_reload(const char *name); /* unload (kalau ada) + load */
void fs_load_configured(const char *list); /* "fs1 fs2" dari server.cfg */
int  fs_count(void);
const char *fs_get_name(int idx); /* idx 0..fs_count()-1 */

/* Event gamemode. Kembalikan 1 = handled (lewati default C), 0 = pakai default. */
int  lua_on_player_join(int id, const char *name);
int  lua_on_player_leave(int id, const char *name, int reason);
int  lua_on_player_chat(int id, const char *name, const char *text);
int  lua_on_player_command(int id, const char *name, const char *cmd, const char *args);
/* Callback NUI dari client (selalu dianggap handled, tak ada default C). */
int  lua_on_nui_callback(int id, const char *name, const char *data);
/* Event console: true = di-handle gamemode. */
int  lua_on_console_command(const char *cmd, const char *args);
/* Event pre-connect: true = tolak (reason_out diisi alasan). */
int  lua_on_player_request(const char *name, const char *ip,
                           char *reason_out, size_t reason_sz);
/* Event kendaraan ala SA-MP (tidak ada default C; gamemode opsional). */
void lua_on_player_enter_vehicle(int pid, int vid, int seat);
void lua_on_player_exit_vehicle(int pid, int vid);
void lua_on_vehicle_damage(int vid, int damagerid, int newhealth);
void lua_on_vehicle_death(int vid, int killerid);
void lua_on_vehicle_spawn(int vid);
/* Event objek ala SA-MP (tidak ada default C; gamemode opsional):
 * onObjectMoved(objid) — menyala saat MoveObject tiba di target. */
void lua_on_object_moved(int objid);
/* Event actor ala SA-MP (tidak ada default C; gamemode opsional):
 * onPlayerGiveDamageActor(pid, actorid, amount, weaponid, bodypart) — bodypart
 * selalu 0 (client tak melaporkannya); onActorDeath(actorid, killerid) —
 * menyala saat damage membuat health actor mencapai 0. */
void lua_on_player_give_damage_actor(int pid, int actorid, float amount,
                                     int weaponid);
void lua_on_actor_death(int actorid, int killerid);
/* Event senjata ala SA-MP (tidak ada default C; gamemode opsional).
 * Mirip OnPlayerWeaponShot: onPlayerWeaponShot(pid, weaponid, hittype,
 * hitid, x, y, z). */
void lua_on_player_weapon_shot(int pid, int weaponid, int hittype, int hitid,
                               float x, float y, float z);
/* Event kill feed ala SA-MP (tidak ada default C; gamemode opsional).
 * Mirip OnPlayerDeath: onPlayerDeath(victimid, killerid, reason) — menyala
 * hanya untuk laporan kematian dari client (0x46), TIDAK untuk
 * romp.sendDeathMessage manual (ala SA-MP SendDeathMessage tak menyalakan
 * OnPlayerDeath). killerid 255 = tak ada pembunuh. */
void lua_on_player_death(int victimid, int killerid, int reason);
/* Event wanted level ala SA-MP (perpanjangan RO-MP): menyala setiap
 * romp.setPlayerWantedLevel berhasil — onPlayerWantedLevelChange(pid,
 * oldlevel, newlevel). Dipakai gamemode untuk logika polisi. */
void lua_on_wanted_level_change(int pid, int oldlevel, int newlevel);
/* Event uang pemain ala SA-MP (perpanjangan RO-MP): menyala setiap
 * romp.setPlayerMoney/givePlayerMoney/resetPlayerMoney berhasil —
 * onPlayerMoneyChange(pid, oldamount, newamount). Dipakai gamemode untuk
 * logika bank/payday. */
void lua_on_player_money_change(int pid, int32_t oldamount, int32_t newamount);
/* Event spectate ala SA-MP (perpanjangan RO-MP): menyala setiap status
 * spectate berubah — onPlayerSpectate(pid, spectating, targettype, target,
 * mode). spectating 0/1; targettype 0=none/1=pemain/2=kendaraan;
 * target = pid/vid (0 bila none); mode 0=normal/1=fixed/2=side. */
void lua_on_player_spectate(int pid, int spectating, int targettype,
                            int target, int mode);
/* Event key state ala SA-MP: menyala setiap state tombol pemain berubah
 * (0x4C KEY_STATE) — onPlayerKeyStateChange(pid, newkeys, oldkeys).
 * newkeys/oldkeys = bitmask KEY_* (romp_key.h), 0 = tak ada tombol ditekan. */
void lua_on_player_key_change(int pid, uint16_t newkeys, uint16_t oldkeys);
/* Event animasi pemain ala SA-MP: menyala saat ApplyAnimation diterapkan
 * — onPlayerApplyAnimation(pid, animlib, animname); dan saat dihentikan —
 * onPlayerClearAnimations(pid). Perpanjangan RO-MP (SA-MP tak punya
 * callback ini), untuk logika gamemode (mis. /me otomatis saat menari). */
void lua_on_player_apply_animation(int pid, const char *animlib,
                                   const char *animname);
void lua_on_player_clear_animations(int pid);
/* Event interior & virtual world ala SA-MP (tidak ada default C):
 * onPlayerInteriorChange(pid, old, new),
 * onPlayerVirtualWorldChange(pid, old, new). */
void lua_on_player_interior_change(int pid, int old_int, int new_int);
void lua_on_player_vw_change(int pid, uint32_t old_vw, uint32_t new_vw);
/* Event anti-cheat dasar ala SA-MP (tidak ada default C):
 * onPlayerCheat(pid, type, value) — type: "teleport"|"speed"|"health"|"armour". */
void lua_on_player_cheat(int pid, const char *type, double value);
/* Dipanggil tiap 1 detik. */
void lua_on_tick(void);
