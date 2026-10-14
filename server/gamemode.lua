-- ============================================================
-- RO-MP Gamemode (Lua) — contoh bawaan, ala FiveM.
--
-- Cara kerja:
--   * Definisikan fungsi event di bawah. Return true = event di-handle
--     oleh gamemode (penanganan default di C dilewati).
--   * Return false / tidak definisikan fungsinya = pakai default C.
--
-- API tersedia di tabel `romp`:
--   romp.log(text)
--   romp.playerCount() -> jumlah pemain
--   romp.getPlayerName(id) -> nama | nil
--   romp.getPlayers() -> array { {id=..., name=...}, ... }
--   romp.sendChatToAll(kind, text)
--   romp.sendChatToPlayer(id, kind, text) -> true/false
--   romp.kickPlayer(id, reason)
--   romp.getTick() -> ms sejak server nyala
--   romp.getConfig("hostname"|"motd"|"port"|"maxplayers")
--   romp.KIND_CHAT / KIND_INFO / KIND_ME / KIND_PM / KIND_SYS
--   NUI (UI ala FiveM):
--   romp.sendNUIMessage(id, jsonString) -> true/false
--   romp.onNUICallback(name, fn) — fn dipanggil sebagai fn(id, name, dataJson)
--   romp.setNUIFocus(id, true/false) -> true/false
--   Info & admin runtime:
--   romp.getPlayerIP(id) -> ip | nil
--   romp.setHostname(text), romp.setMOTD(text), romp.setPassword(text)
--   romp.getServerInfo() -> {hostname, motd, port, maxplayers, players,
--                            hasPassword, query, announce}
--   romp.banIP(ip), romp.unbanIP(ip), romp.isIPBanned(ip) -> bool
--   romp.announceNow() — paksa announce ke master server sekarang
--
-- Event tambahan:
--   onGamemodeInit() — dipanggil sekali saat gamemode dimuat
--   onConsoleCommand(cmd, args) — return true = di-handle (tambah command console)
--   onPlayerRequestConnect(name, ip) — return true, "alasan" = tolak koneksi
-- ============================================================

-- Helper JSON untuk pesan NUI (server dijalankan dari folder server/,
-- jadi json.lua ada di sebelah gamemode.lua).
local json = dofile("json.lua")

-- ============================================================
-- DEMO NUI: inventory ala ox_inventory. Ketik /inv di dalam game.
-- Halaman UI: android/.../assets/romp/nui/inventory.html
-- ============================================================
local demoItems = {
    { id = "roti", name = "Roti",        count = 3 },
    { id = "air",  name = "Air Mineral", count = 2 },
    { id = "kopi", name = "Kopi",        count = 1 },
    { id = "obat", name = "Obat",        count = 1 },
    { id = "kunci", name = "Kunci",      count = 1 },
}

local function bukaInventory(id)
    romp.sendNUIMessage(id, json.encode({
        action = "openInv",
        items = demoItems,
    }))
    romp.setNUIFocus(id, true)
    romp.sendChatToPlayer(id, romp.KIND_INFO,
        "*** Inventory terbuka (demo NUI).")
end

-- JS -> Lua: pemain memakai item dari UI.
romp.onNUICallback("nui_inv_use", function(pid, name, data)
    local ok, d = pcall(json.decode, data)
    local itemName = (ok and d and d.name) or data
    romp.sendChatToPlayer(pid, romp.KIND_INFO,
        "*** Kamu memakai: " .. tostring(itemName))
    romp.log(name .. " memakai item NUI: " .. tostring(itemName))
end)

-- JS -> Lua: pemain menutup UI.
romp.onNUICallback("nui_inv_close", function(pid, name, data)
    romp.setNUIFocus(pid, false)
end)

romp.log("Gamemode dimuat untuk server: " .. romp.getConfig("hostname"))

-- Pemain join. Default C: "*** <nama> (id) bergabung."
function onPlayerJoin(id, name)
    romp.sendChatToAll(romp.KIND_SYS,
        "*** " .. name .. " (" .. id .. ") datang ke kota. Selamat main!")
    return true
end

-- Pemain leave. reason: 0=quit, 1=kick, 2=timeout.
-- Default C: "*** <nama> (id) keluar/di-kick/timeout."
function onPlayerLeave(id, name, reason)
    local rs = reason == 1 and "di-kick" or reason == 2 and "timeout" or "keluar"
    romp.sendChatToAll(romp.KIND_SYS,
        "*** " .. name .. " (" .. id .. ") " .. rs .. ".")
    return true
end

-- Command custom. cmd = tanpa slash, args = sisa teks.
-- Return false supaya command bawaan (/me /pm /players /help /q) tetap jalan.
function onPlayerCommand(id, name, cmd, args)
    if cmd == "waktu" then
        romp.sendChatToPlayer(id, romp.KIND_INFO,
            "*** Waktu server: " .. os.date("%H:%M:%S"))
        return true
    elseif cmd == "info" then
        romp.sendChatToPlayer(id, romp.KIND_INFO,
            "*** " .. romp.getConfig("hostname")
            .. " | Online: " .. romp.playerCount()
            .. "/" .. romp.getConfig("maxplayers"))
        return true
    elseif cmd == "inv" then
        bukaInventory(id)
        return true
    elseif cmd == "myip" then
        romp.sendChatToPlayer(id, romp.KIND_INFO,
            "*** IP kamu: " .. tostring(romp.getPlayerIP(id)))
        return true
    elseif cmd == "serverinfo" then
        local si = romp.getServerInfo()
        romp.sendChatToPlayer(id, romp.KIND_INFO,
            string.format("*** %s | %d/%d pemain | pass:%s query:%s",
                si.hostname, si.players, si.maxplayers,
                tostring(si.hasPassword), tostring(si.query)))
        return true
    elseif cmd == "sethost" then
        if args ~= "" then
            romp.setHostname(args)
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Hostname diganti jadi: " .. args)
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Pakai: /sethost <nama baru>")
        end
        return true
    elseif cmd == "kick" then
        -- contoh command admin sederhana: /kick <id> <alasan>
        local target, alasan = args:match("^(%d+)%s*(.*)$")
        target = tonumber(target)
        if target then
            romp.kickPlayer(target, alasan)
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO, "*** Pakai: /kick <id> [alasan]")
        end
        return true
    end
    return false
end

-- Filter chat sederhana. Return true = pesan diblokir (tidak di-broadcast).
-- Ganti daftar kata di bawah sesuai kebutuhan server kalian.
local kataTerlarang = {
    -- "contoh1", "contoh2",
}

function onPlayerChat(id, name, text)
    local lower = text:lower()
    for _, kata in ipairs(kataTerlarang) do
        if lower:find(kata, 1, true) then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Pesanmu diblokir filter kata.")
            romp.log("Filter memblokir chat dari " .. name)
            return true
        end
    end
    return false
end

-- Dipanggil sekali saat gamemode dimuat (sebelum server menerima pemain).
function onGamemodeInit()
    romp.log("Gamemode init: " .. romp.getConfig("hostname")
        .. " | query=" .. tostring(romp.getServerInfo().query))
end

-- Command console custom. Contoh: ketik 'announce' di console server.
function onConsoleCommand(cmd, args)
    if cmd == "announce" then
        romp.announceNow()
        romp.log("Announce ke master dipaksa via console.")
        return true
    end
    return false
end

-- Dipanggil SEBELUM handshake selesai. Return true + alasan = tolak koneksi.
-- Contoh: tolak nickname yang dilarang.
function onPlayerRequestConnect(name, ip)
    if name:lower() == "badguy" then
        return true, "Nickname dilarang di server ini."
    end
    return false
end

-- Dipanggil tiap 1 detik. Cocok untuk timer gamemode.
local detik = 0

function onTick()
    detik = detik + 1
    -- tiap 5 menit: ingatkan MOTD
    if detik % 300 == 0 then
        romp.sendChatToAll(romp.KIND_INFO, "*** " .. romp.getConfig("motd"))
    end
end
