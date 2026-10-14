-- ============================================================
-- Contoh filterscript RO-MP ala SA-MP.
--
-- Taruh file ini di: server/filterscripts/contoh.lua
-- Daftar di server.cfg:  filterscripts contoh
-- Atau load/unload saat server jalan (console / RCON):
--   loadfs contoh | unloadfs contoh | reloadfs contoh | filterscripts
--
-- Tiap filterscript punya state Lua SENDIRI (global terpisah dari
-- gamemode & filterscript lain) tapi API romp.* yang sama, termasuk
-- helper (dialog, textdraw, checkpoint, pickup, mapicon, vehicle).
--
-- Event dikirim ke filterscript DULU (urutan load), baru gamemode.
-- Kalau filterscript return true (= handled), gamemode dilewati.
-- Lifecycle: onFilterScriptInit() saat load, onFilterScriptExit() saat unload.
-- ============================================================

function onFilterScriptInit()
    romp.log("[contoh] filterscript dimuat — ketik /fsinfo di chat")
end

function onFilterScriptExit()
    romp.log("[contoh] filterscript di-unload, dadah!")
end

function onPlayerJoin(pid, name)
    romp.sendChatToPlayer(pid, romp.KIND_INFO,
        "[contoh] Selamat datang, " .. name .. "! (filterscript 'contoh' aktif)")
end

function onPlayerCommand(pid, name, cmd, args)
    if cmd == "fsinfo" then
        local fs = romp.getFilterScripts()
        local daftar = #fs > 0 and table.concat(fs, ", ") or "(kosong)"
        romp.sendChatToPlayer(pid, romp.KIND_INFO,
            "[contoh] Filterscript aktif: " .. daftar)
        return true -- handled: gamemode tidak dipanggil untuk /fsinfo
    end
    return false
end
