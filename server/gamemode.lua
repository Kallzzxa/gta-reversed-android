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
--   Dialog ala SA-MP (DIALOG_STYLE_MSGBOX/LIST/INPUT/PASSWORD):
--   romp.showDialog(id, dialogId, style, caption, info, button1, button2)
--       -> true/false (otomatis fokus NUI on)
--   romp.hideDialog(id) -> true/false (otomatis fokus NUI off)
--   romp.DIALOG_MSGBOX / DIALOG_INPUT / DIALOG_LIST / DIALOG_PASSWORD
--   Event: onDialogResponse(id, name, dialogId, response, listitem, inputtext)
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
--   Checkpoint ala SA-MP:
--   romp.createCheckpoint(ctype, x, y, z, size) -> id | nil
--       (romp.CHECKPOINT_NORMAL / romp.CHECKPOINT_RACE)
--   romp.destroyCheckpoint(id) -> bool
--   romp.showCheckpointForPlayer(pid, id) -> bool
--   romp.hideCheckpointForPlayer(pid) -> bool
--   romp.getPlayerCheckpoint(pid) -> id | nil
--   romp.setCheckpointPos(id, x, y, z[, size]) -> bool
--   romp.setRaceCheckpointNext(id, nx, ny, nz) -> bool
--   romp.getCheckpointPos(id) -> x, y, z, size | nil
--   romp.getPlayerPos(id) -> x, y, z | nil
--   romp.isPlayerInCheckpoint(pid) -> bool
--   Event: onPlayerEnterCheckpoint(pid, cpid),
--          onPlayerLeaveCheckpoint(pid, cpid),
--          onPlayerEnterRaceCheckpoint(pid, cpid),
--          onPlayerLeaveRaceCheckpoint(pid, cpid)
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

-- ============================================================
-- DEMO DIALOG ala SA-MP. Ketik /dialog di dalam game.
-- Halaman UI: android/.../assets/romp/nui/dialog.html
-- Jawaban pemain masuk ke onDialogResponse di bawah.
-- ============================================================

-- Pemain menjawab dialog. dialogId sesuai yang dikirim showDialog.
function onDialogResponse(id, name, dialogId, response, listitem, inputtext)
    if dialogId == 100 then
        -- Menu utama (LIST). Klik item = response true.
        if not response then return end
        if listitem == 0 then
            local si = romp.getServerInfo()
            romp.showDialog(id, 101, romp.DIALOG_MSGBOX, "Info Server",
                "Hostname: " .. si.hostname
                .. "\nPemain: " .. si.players .. "/" .. si.maxplayers
                .. "\nPassword: " .. (si.hasPassword and "ya" or "tidak"),
                "OK", "")
        elseif listitem == 1 then
            romp.showDialog(id, 102, romp.DIALOG_INPUT, "Ganti Nickname",
                "Masukkan nickname baru:", "Ganti", "Batal")
        end
    elseif dialogId == 102 and response then
        -- Hasil input nickname.
        if inputtext ~= "" then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Nickname diganti jadi: " .. inputtext .. " (demo dialog)")
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Nickname tidak boleh kosong.")
        end
    end
end

-- ============================================================
-- DEMO TEXTDRAW ala SA-MP. Ketik /td, /tdstr, /tdhide, /ptd di game.
-- Halaman UI: android/.../assets/romp/nui/textdraw.html
-- ============================================================
local demoTds = {}  -- id textdraw global demo
local demoLabel = {}  -- id pemain -> id 3D text label demo

local function hapusDemoTd()
    for _, tdid in ipairs(demoTds) do romp.destroyTextDraw(tdid) end
    demoTds = {}
end

local function tampilDemoTd()
    hapusDemoTd()
    local banner = romp.createTextDraw(320, 16, "RO-MP SERVER")
    romp.setTextDrawStyle(banner,
        { color = 0xFFD700FF, size = 2.2, align = 2, box = 1 })
    romp.showTextDraw(banner)
    demoTds[#demoTds + 1] = banner

    local sub = romp.createTextDraw(320, 64, "textdraw demo ala SA-MP\n/global & per-pemain")
    romp.setTextDrawStyle(sub, { color = 0xFFFFFFFF, size = 1.0, align = 2 })
    romp.showTextDraw(sub)
    demoTds[#demoTds + 1] = sub

    local hint = romp.createTextDraw(8, 448, "/tdstr <teks> ganti banner | /tdhide sembunyikan")
    romp.setTextDrawStyle(hint, { color = 0xAAAAAAFF, size = 0.8 })
    romp.showTextDraw(hint)
    demoTds[#demoTds + 1] = hint
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
    -- Label yang di-attach sudah dihapus otomatis oleh server,
    -- tapi referensi demo tetap dibersihkan.
    demoLabel[id] = nil
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
    elseif cmd == "dialog" then
        romp.showDialog(id, 100, romp.DIALOG_LIST, "Menu Demo",
            "Info Server\nGanti Nickname", "Pilih", "Batal")
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
    elseif cmd == "td" then
        tampilDemoTd()
        romp.sendChatToPlayer(id, romp.KIND_INFO,
            "*** 3 textdraw global ditampilkan untuk semua pemain.")
        return true
    elseif cmd == "tdstr" then
        if args ~= "" and demoTds[1] then
            romp.setTextDrawString(demoTds[1], args)
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Banner diganti jadi: " .. args)
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Pakai: /td dulu, lalu /tdstr <teks>")
        end
        return true
    elseif cmd == "tdhide" then
        for _, tdid in ipairs(demoTds) do romp.hideTextDraw(tdid) end
        romp.sendChatToPlayer(id, romp.KIND_INFO,
            "*** Textdraw demo disembunyikan.")
        return true
    elseif cmd == "ptd" then
        -- Textdraw per-pemain: hanya yang mengetik yang melihat.
        local pt = romp.createPlayerTextDraw(id, 320, 400,
            "Halo, " .. name .. "! (hanya kamu yang lihat ini)")
        romp.setPlayerTextDrawStyle(pt,
            { color = 0x7CFC00FF, size = 1.1, align = 2, box = 1 })
        romp.showPlayerTextDraw(id, pt)
        romp.sendChatToPlayer(id, romp.KIND_INFO,
            "*** Textdraw per-pemain ditampilkan (id " .. pt .. ").")
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
    elseif cmd == "scoreboard" or cmd == "sb" then
        -- Scoreboard ala SA-MP (TAB): toggle tampil/sembunyi + lihat ping sendiri.
        if romp.isScoreboardVisible(id) then
            romp.toggleScoreboard(id, false)
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Scoreboard disembunyikan.")
        else
            romp.toggleScoreboard(id, true)
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Scoreboard ditampilkan. Ping kamu: " ..
                tostring(romp.getPlayerPing(id)) .. " ms.")
        end
        return true
    elseif cmd == "bubble" then
        -- Chat bubble ala SA-MP SetPlayerChatBubble: /bubble <teks>
        if args == "" then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Pakai: /bubble <teks>")
        else
            romp.setPlayerChatBubble(id, args, 0xFFFFFFFF, 25.0, 5000)
        end
        return true
    elseif cmd == "tp" then
        -- Teleport ala SA-MP SetPlayerPos: /tp <x> <y> <z>
        local x, y, z = args:match("^([%-%d%.]+)%s+([%-%d%.]+)%s+([%-%d%.]+)%s*$")
        x, y, z = tonumber(x), tonumber(y), tonumber(z)
        if x and y and z then
            if romp.setPlayerPos(id, x, y, z) then
                romp.sendChatToPlayer(id, romp.KIND_INFO,
                    string.format("*** Teleport ke (%.1f, %.1f, %.1f).", x, y, z))
            else
                romp.sendChatToPlayer(id, romp.KIND_INFO,
                    "*** Gagal teleport (id pemain tidak valid).")
            end
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Pakai: /tp <x> <y> <z>")
        end
        return true
    elseif cmd == "label" then
        -- 3D text label ala SA-MP (Create3DTextLabel): /label <teks>
        -- Label statis dibuat di posisi pemain, lalu di-attach ke pemain
        -- supaya mengikuti ke mana pun ia pergi.
        if args == "" then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Pakai: /label <teks>")
            return true
        end
        local x, y, z = romp.getPlayerPos(id)
        if not x then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Posisi kamu belum diketahui server.")
            return true
        end
        local lid = romp.create3DTextLabel(args, 0xFFFF00FF, x, y, z + 1.0, 80.0)
        if lid and romp.attach3DTextLabelToPlayer(lid, id) then
            demoLabel[id] = lid
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Label 3D dibuat & ditempel ke kamu (id " .. lid .. ").")
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Gagal membuat label.")
        end
        return true
    elseif cmd == "labeldel" then
        -- Hapus label demo milik pemain.
        local lid = demoLabel[id]
        if lid and romp.delete3DTextLabel(lid) then
            demoLabel[id] = nil
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Label 3D dihapus.")
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Kamu belum punya label (/label dulu).")
        end
        return true
    elseif cmd == "gz" then
        -- GangZone ala SA-MP (kotak warna di radar):
        -- /gz create <minx> <miny> <maxx> <maxy>
        -- /gz show <id> [rrggbbaa-hex] | /gz showall <id> [hex]
        -- /gz flash <id> <hex> | /gz stopflash <id>
        -- /gz hide <id> | /gz hideall <id> | /gz destroy <id> | /gz list
        local sub, rest = args:match("^(%S+)%s*(.*)$")
        sub = sub or ""
        if sub == "create" then
            local a, b, c, d = rest:match(
                "^([%d%.%-]+)%s+([%d%.%-]+)%s+([%d%.%-]+)%s+([%d%.%-]+)")
            a, b, c, d = tonumber(a), tonumber(b), tonumber(c), tonumber(d)
            if not a then
                romp.sendChatToPlayer(id, romp.KIND_INFO,
                    "*** Pakai: /gz create <minx> <miny> <maxx> <maxy>")
                return true
            end
            local zid = romp.createGangZone(a, b, c, d)
            romp.sendChatToPlayer(id, romp.KIND_INFO, zid and
                ("*** GangZone dibuat (id " .. zid .. "). /gz show " ..
                 zid .. " untuk menampilkannya.") or
                "*** Gagal membuat GangZone (penuh?).")
        elseif sub == "show" or sub == "showall" or sub == "flash"
            or sub == "hide" or sub == "hideall" or sub == "destroy"
            or sub == "stopflash" then
            local zid = tonumber((rest:match("^(%d+)")))
            if not zid then
                romp.sendChatToPlayer(id, romp.KIND_INFO,
                    "*** Pakai: /gz " .. sub .. " <id> [warna-hex]")
                return true
            end
            local hex = rest:match("%s+([0-9A-Fa-f]+)%s*$")
            local color = hex and tonumber(hex, 16)
            local ok
            if sub == "show" then
                ok = romp.showGangZoneForPlayer(id, zid, color or 0xFF0000FF)
            elseif sub == "showall" then
                ok = romp.showGangZoneForAll(zid, color or 0xFF0000FF)
            elseif sub == "flash" then
                ok = romp.flashGangZoneForPlayer(id, zid,
                    color or 0xFFFFFFFF)
            elseif sub == "stopflash" then
                ok = romp.stopGangZoneFlashForPlayer(id, zid)
            elseif sub == "hide" then
                ok = romp.hideGangZoneForPlayer(id, zid)
            elseif sub == "hideall" then
                ok = romp.hideGangZoneForAll(zid)
            else
                ok = romp.destroyGangZone(zid)
            end
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** /gz " .. sub .. " " .. zid .. ": " .. tostring(ok))
        elseif sub == "list" then
            local parts = {}
            for zid, v in pairs(romp.getVisibleGangZones(id)) do
                parts[#parts + 1] = zid .. (v.flash ~= 0 and "*" or "")
            end
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** GangZone terlihat: " ..
                (#parts > 0 and table.concat(parts, ", ") or "-"))
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Pakai: /gz create|show|showall|flash|stopflash|" ..
                "hide|hideall|destroy|list")
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
