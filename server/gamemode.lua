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
-- ============================================================

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

-- Dipanggil tiap 1 detik. Cocok untuk timer gamemode.
local detik = 0

function onTick()
    detik = detik + 1
    -- tiap 5 menit: ingatkan MOTD
    if detik % 300 == 0 then
        romp.sendChatToAll(romp.KIND_INFO, "*** " .. romp.getConfig("motd"))
    end
end
