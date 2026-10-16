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

-- Helper format rupiah ("Rp1.000.000").
local function rp(n)
    n = math.floor(n or 0)
    local neg = n < 0
    local s = tostring(math.abs(n))
    local out = ""
    while #s > 3 do
        out = "." .. s:sub(-3) .. out
        s = s:sub(1, -4)
    end
    out = s .. out
    return (neg and "-Rp" or "Rp") .. out
end

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
local demoObj = {}  -- id pemain -> id objek demo
local demoActor = {}  -- id pemain -> id actor demo
local demoVeh = {}    -- id pemain -> id kendaraan modif demo

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

-- Key state ala SA-MP (OnPlayerKeyStateChange): perpanjangan RO-MP —
-- event ini menyala setiap state tombol pemain berubah (0x4C KEY_STATE).
-- Demo: umumkan tombol yang BARU ditekan (bukan yang dilepas) ke chat.
local KEY_NAMES = {
    [romp.KEY_ACTION] = "ACTION", [romp.KEY_CROUCH] = "CROUCH",
    [romp.KEY_FIRE] = "FIRE", [romp.KEY_SPRINT] = "SPRINT",
    [romp.KEY_SECONDARY_ATTACK] = "2ND-ATTACK", [romp.KEY_JUMP] = "JUMP",
    [romp.KEY_LOOK_RIGHT] = "LOOK-R", [romp.KEY_HANDBRAKE] = "HANDBRAKE",
    [romp.KEY_LOOK_LEFT] = "LOOK-L", [romp.KEY_SUBMISSION] = "SUBMISSION",
    [romp.KEY_WALK] = "WALK", [romp.KEY_ANALOG_UP] = "ANALOG-UP",
    [romp.KEY_ANALOG_DOWN] = "ANALOG-DOWN",
    [romp.KEY_ANALOG_RIGHT] = "ANALOG-RIGHT",
    [romp.KEY_ANALOG_LEFT] = "ANALOG-L", [romp.KEY_YES] = "YES",
    [romp.KEY_NO] = "NO", [romp.KEY_CTRL_BACK] = "CTRL-BACK",
}
local KEY_ORDER = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024,
    2048, 4096, 8192, 16384, 65536, 131072, 262144 }
local function key_names(keys)
    local parts = {}
    for _, k in ipairs(KEY_ORDER) do
        -- tanpa bit32 di Lua 5.4: cek bit via aritmetika
        if math.floor(keys / k) % 2 == 1 then
            parts[#parts + 1] = KEY_NAMES[k] or ("0x" .. string.format("%X", k))
        end
    end
    return #parts > 0 and table.concat(parts, "+") or "-"
end
function onPlayerKeyStateChange(pid, newkeys, oldkeys)
    -- hanya yang baru ditekan (bit 0->1); pelepas tak diumumkan
    local pressed = 0
    for _, k in ipairs(KEY_ORDER) do
        local n = math.floor(newkeys / k) % 2
        local o = math.floor(oldkeys / k) % 2
        if n == 1 and o == 0 then pressed = pressed + k end
    end
    if pressed ~= 0 then
        local name = romp.getPlayerName(pid) or ("id " .. pid)
        romp.sendChatToAll(romp.KIND_SYS,
            "*** ⌨ " .. name .. " menekan: " .. key_names(pressed))
    end
    return true
end

-- Animasi pemain ala SA-MP: perpanjangan RO-MP (SA-MP tak punya callback).
-- Contoh: /me otomatis saat menari supaya RP hidup.
function onPlayerApplyAnimation(pid, animlib, animname)
    if animlib == "DANCING" then
        local name = romp.getPlayerName(pid) or ("id " .. pid)
        romp.sendChatToAll(romp.KIND_ME,
            "*** " .. name .. " mulai menari (" .. animname .. ")")
    end
    return true
end

function onPlayerClearAnimations(pid)
    local name = romp.getPlayerName(pid) or ("id " .. pid)
    romp.log("Animasi " .. name .. " (id " .. pid .. ") dihentikan")
    return true
end

-- Kill feed ala SA-MP: menyala hanya untuk laporan kematian dari client
-- (paket 0x46), TIDAK untuk /deathmsg (romp.sendDeathMessage manual).
function onPlayerDeath(victimid, killerid, reason)
    local vname = romp.getPlayerName(victimid) or ("id " .. victimid)
    local line
    if killerid == 255 then
        line = "*** " .. vname .. " mati (" .. reason .. ")."
    else
        local kname = romp.getPlayerName(killerid) or ("id " .. killerid)
        line = "*** " .. kname .. " membunuh " .. vname
            .. " (senjata " .. reason .. ")."
    end
    romp.sendChatToAll(romp.KIND_SYS, line)
    return true
end

-- Wanted level ala SA-MP: perpanjangan RO-MP — event ini TIDAK ada di
-- SA-MP, tapi memudahkan logika polisi di gamemode (mobil patroli,
-- skin polisi, dsb).
function onPlayerWantedLevelChange(pid, oldlevel, newlevel)
    local name = romp.getPlayerName(pid) or ("id " .. pid)
    if newlevel > oldlevel and newlevel >= 3 then
        -- Logika "cop car" demo: wanted tinggi = polisi dikerahkan.
        romp.sendChatToAll(romp.KIND_SYS,
            "*** 🚔 PERINGATAN: " .. name .. " kini wanted level "
            .. newlevel .. "! Polisi mengerahkan mobil patroli.")
    elseif newlevel == 0 and oldlevel > 0 then
        romp.sendChatToAll(romp.KIND_SYS,
            "*** " .. name .. " sudah bersih (wanted 0).")
    else
        romp.sendChatToPlayer(pid, romp.KIND_INFO,
            "*** Wanted level kamu sekarang: " .. newlevel .. ".")
    end
    return true
end

-- Tim pemain ala SA-MP: perpanjangan RO-MP — event ini TIDAK ada di
-- SA-MP, tapi memudahkan logika gangwar/dm di gamemode (warna marker,
-- respawn di base tim, dsb).
function onPlayerTeamChange(pid, oldteam, newteam)
    local name = romp.getPlayerName(pid) or ("id " .. pid)
    local function teamname(t)
        if t == romp.TEAM_NONE then return "tanpa tim" end
        return "tim " .. t
    end
    romp.sendChatToAll(romp.KIND_SYS,
        "*** ⚔️ " .. name .. " pindah tim: "
        .. teamname(oldteam) .. " -> " .. teamname(newteam) .. ".")
    return true
end

-- Uang pemain otoritatif ala SA-MP: perpanjangan RO-MP — event ini TIDAK
-- ada di SA-MP, tapi memudahkan logika bank/payday di gamemode (catat
-- transaksi besar, dsb).
function onPlayerMoneyChange(pid, oldamount, newamount)
    local name = romp.getPlayerName(pid) or ("id " .. pid)
    if math.abs(newamount - oldamount) >= 1000000 then
        -- Transaksi besar = umumkan ala berita ekonomi RP.
        romp.sendChatToAll(romp.KIND_SYS,
            "*** 💸 Transaksi besar: " .. name .. " "
            .. rp(oldamount) .. " -> " .. rp(newamount) .. ".")
    end
    return true
end

-- Spectate ala SA-MP: perpanjangan RO-MP — event ini TIDAK ada di SA-MP.
-- Menyala setiap status spectate berubah (mulai/berhenti/ganti target).
function onPlayerSpectate(pid, spectating, targettype, target, mode)
    local name = romp.getPlayerName(pid) or ("id " .. pid)
    if spectating == 1 then
        local tname = targettype == 1 and "pemain" or
                      targettype == 2 and "kendaraan" or "tanpa target"
        romp.sendChatToAll(romp.KIND_SYS,
            "*** 👁 " .. name .. " mulai spectate " .. tname ..
            (targettype ~= 0 and (" (id " .. target .. ")") or "") .. ".")
    else
        romp.sendChatToAll(romp.KIND_SYS,
            "*** 👁 " .. name .. " berhenti spectate.")
    end
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
    demoObj[id] = nil
    demoActor[id] = nil
    demoVeh[id] = nil
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
    elseif cmd == "playaudio" then
        -- Audio stream ala SA-MP (PlayAudioStreamForPlayer):
        -- /playaudio <url> -> putar global; /playaudio <url> pos -> posisional
        -- di posisi pemain (jarak dengar 50); /playaudio <url> loop -> ulang.
        local url, mode = args:match("^(%S+)%s*(%S*)$")
        if not url or url == "" then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Pakai: /playaudio <url mp3/ogg> [pos|loop]")
            return true
        end
        local usepos, loop = 0, 0
        if mode == "pos" then usepos = 1 elseif mode == "loop" then loop = 1 end
        local x, y, z = romp.getPlayerPos(id)
        if romp.playAudioStreamForPlayer(id, url, x or 0, y or 0, z or 0,
                                        50.0, usepos, loop) then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Audio diputar: " .. url)
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Gagal memutar audio.")
        end
        return true
    elseif cmd == "stopaudio" then
        -- Hentikan audio stream (ala SA-MP StopAudioStreamForPlayer).
        if romp.stopAudioStreamForPlayer(id) then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Audio dihentikan.")
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Tidak ada audio yang diputar (/playaudio dulu).")
        end
        return true
    elseif cmd == "object" then
        -- Objek ala SA-MP (CreateObject/MoveObject):
        -- /object [modelid] -> buat di posisi pemain, /object move -> geser,
        -- /objdel -> hapus.
        local sub = args:match("^(%S+)") or ""
        local x, y, z = romp.getPlayerPos(id)
        if not x then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Posisi kamu belum diketahui server.")
            return true
        end
        if sub == "move" then
            local oid = demoObj[id]
            if oid and romp.moveObject(oid, x + 10.0, y, z, 5.0) then
                romp.sendChatToPlayer(id, romp.KIND_INFO,
                    "*** Objek digeser 10 unit ke depan (5 unit/detik).")
            else
                romp.sendChatToPlayer(id, romp.KIND_INFO,
                    "*** Kamu belum punya objek (/object dulu).")
            end
            return true
        end
        local modelid = tonumber(sub) or 1337
        if demoObj[id] then romp.destroyObject(demoObj[id]) end
        local oid = romp.createObject(modelid, x + 2.0, y, z, 0.0, 0.0, 0.0)
        if oid then
            demoObj[id] = oid
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Objek dibuat (id " .. oid .. ", model " .. modelid ..
                "). /object move = geser, /objdel = hapus.")
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Gagal membuat objek.")
        end
        return true
    elseif cmd == "objdel" then
        -- Hapus objek demo milik pemain.
        local oid = demoObj[id]
        if oid and romp.destroyObject(oid) then
            demoObj[id] = nil
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Objek dihapus.")
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Kamu belum punya objek (/object dulu).")
        end
        return true
    elseif cmd == "actor" then
        -- Actor statis ala SA-MP (CreateActor): /actor [modelid]
        -- Dibuat di depan pemain + animasi joget, /actordel = hapus.
        local x, y, z = romp.getPlayerPos(id)
        if not x then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Posisi kamu belum diketahui server.")
            return true
        end
        local modelid = tonumber(args) or 7  -- 7 = skin bandit
        if demoActor[id] then romp.destroyActor(demoActor[id]) end
        local aid = romp.createActor(modelid, x + 3.0, y, z, 180.0)
        if aid then
            demoActor[id] = aid
            romp.applyActorAnimation(aid, "DANCING:DNCE_M_a")
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Actor dibuat (id " .. aid .. ", model " .. modelid ..
                "). /actordel = hapus.")
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Gagal membuat actor (model 0-311).")
        end
        return true
    elseif cmd == "actordel" then
        -- Hapus actor demo milik pemain.
        local aid = demoActor[id]
        if aid and romp.destroyActor(aid) then
            demoActor[id] = nil
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Actor dihapus.")
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Kamu belum punya actor (/actor dulu).")
        end
        return true
    elseif cmd == "attach" then
        -- Attached object ala SA-MP (SetPlayerAttachedObject):
        -- /attach <index 0-9> <modelid> [bone 1-18] = tempelkan objek
        -- di bone tangan kanan (6) dengan offset kecil. /detach = lepas.
        local idx, model, bone = args:match("(%d+) (%d+)%s*(%d*)")
        idx, model = tonumber(idx), tonumber(model)
        bone = tonumber(bone) or 6
        if not idx or not model then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Pakai: /attach <index 0-9> <modelid> [bone 1-18].")
            return true
        end
        if romp.setPlayerAttachedObject(id, idx, model, bone,
                0.1, 0.2, 0.3, 0, 0, 90, 1, 1, 1) then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Objek " .. model .. " ditempel di slot " .. idx ..
                " (bone " .. bone .. "). /detach " .. idx .. " = lepas.")
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Gagal: index 0-9, modelid != 0, bone 1-18.")
        end
        return true
    elseif cmd == "detach" then
        -- Lepaskan attached object (RemovePlayerAttachedObject).
        local idx = tonumber(args)
        if idx and romp.removePlayerAttachedObject(id, idx) then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Objek di slot " .. idx .. " dilepas.")
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Slot kosong / index 0-9. (/attach dulu).")
        end
        return true
    elseif cmd == "skill" then
        -- Weapon skill ala SA-MP (SetPlayerSkillLevel):
        -- /skill <skill 0-10> <level 0-1000>. Nama skill bisa pakai angka
        -- atau nama: pistol, silenced, deagle, shotgun, sawnoff, spas,
        -- uzi, mp5, ak47, m4, sniper.
        local names = {
            pistol = 0, silenced = 1, deagle = 2, shotgun = 3,
            sawnoff = 4, spas = 5, uzi = 6, mp5 = 7, ak47 = 8,
            m4 = 9, sniper = 10,
        }
        local sname = {"Pistol", "Silenced", "Deagle", "Shotgun",
            "Sawnoff", "Spas12", "Uzi", "MP5", "AK47", "M4", "Sniper"}
        local sk, lv = args:match("(%S+) (%-?%d+)")
        sk = tonumber(sk) or names[sk and sk:lower() or ""]
        lv = tonumber(lv)
        if sk == nil or lv == nil then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Pakai: /skill <0-10|nama> <level 0-1000>.")
            return true
        end
        if romp.setPlayerSkillLevel(id, sk, lv) then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Skill " .. (sname[sk + 1] or sk) ..
                " = " .. lv .. ".")
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Gagal: skill 0-10, level 0-1000.")
        end
        return true
    elseif cmd == "skillget" then
        -- Lihat level skill (GetPlayerSkillLevel): /skillget <0-10>.
        local sk = tonumber(args)
        local lv = sk and romp.getPlayerSkillLevel(id, sk)
        romp.sendChatToPlayer(id, romp.KIND_INFO,
            "*** Skill " .. tostring(sk) .. " = " ..
            (lv and tostring(lv) or "? (skill 0-10)") .. ".")
        return true
    elseif cmd == "skills" then
        -- Tampilkan semua 11 level skill pemain.
        local sname = {"Pistol", "Silenced", "Deagle", "Shotgun",
            "Sawnoff", "Spas12", "Uzi", "MP5", "AK47", "M4", "Sniper"}
        local t = romp.getPlayerSkillLevels(id) or {}
        local parts = {}
        for i = 0, 10 do
            parts[#parts + 1] = sname[i + 1] .. "=" .. tostring(t[i + 1])
        end
        romp.sendChatToPlayer(id, romp.KIND_INFO,
            "*** Skill: " .. table.concat(parts, " "))
        return true
    elseif cmd == "markers" then
        -- Mode marker radar ala SA-MP (ShowPlayerMarkers):
        -- /markers <0=off|1=global|2=streamed>.
        local m = tonumber(args)
        if romp.showPlayerMarkers(m) then
            local mn = ({"off", "global", "streamed"})[m + 1] or m
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Mode marker: " .. mn .. ".")
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Pakai: /markers <0|1|2>.")
        end
        return true
    elseif cmd == "pcolor" then
        -- Warna marker pemain ala SA-MP (GetPlayerColor/SetPlayerColor):
        -- /pcolor <pid> <hex RRGGBB|RRGGBBAA>. Tanpa argumen = lihat warna.
        local tp, hx = args:match("(%d+)%s+(%x+)")
        tp = tonumber(tp)
        if not tp then
            local c = romp.getPlayerColor(id)
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                string.format("*** Warna markermu: 0x%08X.", c or 0))
            return true
        end
        local col = tonumber(hx, 16)
        -- hex 6 digit -> tambah alpha FF
        if col and #hx <= 6 then col = col * 256 + 0xFF end
        if col and romp.setPlayerColor(tp, col) then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                string.format("*** Warna marker id %d = 0x%08X.", tp, col))
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Pakai: /pcolor <pid> <hex>, mis. /pcolor 0 FF0000.")
        end
        return true
    elseif cmd == "pmark" then
        -- Override warna marker per-viewer ala SA-MP
        -- (SetPlayerMarkerForPlayer): /pmark <pid> <hex> | /pmark <pid> off.
        local tp, hx = args:match("(%d+)%s+(%S+)")
        tp = tonumber(tp)
        if not tp or not hx then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Pakai: /pmark <pid> <hex|off>.")
            return true
        end
        if hx:lower() == "off" then
            if romp.clearPlayerMarkerForPlayer(id, tp) then
                romp.sendChatToPlayer(id, romp.KIND_INFO,
                    "*** Override marker id " .. tp .. " dihapus.")
            else
                romp.sendChatToPlayer(id, romp.KIND_INFO,
                    "*** Tak ada override untuk id " .. tp .. ".")
            end
            return true
        end
        local col = tonumber(hx, 16)
        if col and #hx <= 6 then col = col * 256 + 0xFF end
        if col and romp.setPlayerMarkerForPlayer(id, tp, col) then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                string.format("*** Menurutmu, marker id %d = 0x%08X.",
                    tp, col))
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Gagal: pid tak valid.")
        end
        return true
    elseif cmd == "deathmsg" then
        -- Demo kill feed ala SA-MP (SendDeathMessage manual):
        -- /deathmsg <killer> <victim> <weapon>.
        -- killer 255 = tak ada pembunuh. Broadcast 0x47 ke semua pemain
        -- TANPA menyalakan event onPlayerDeath (ala SA-MP).
        local k, v, w = args:match("(%d+)%s+(%d+)%s+(%d+)")
        k, v, w = tonumber(k), tonumber(v), tonumber(w)
        if not k or not v or not w then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Pakai: /deathmsg <killer 0-255> <victim> <weapon>.")
            return true
        end
        if romp.sendDeathMessage(k, v, w) then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                string.format("*** Kill feed terkirim: %d [%d] %d.", k, w, v))
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Gagal: id pemain/senjata tak valid.")
        end
        return true
    elseif cmd == "wanted" then
        -- Demo wanted level ala SA-MP (SetPlayerWantedLevel):
        -- /wanted <0-6> -> set wanted sendiri; broadcast 0x48 ke semua
        -- pemain + event onPlayerWantedLevelChange (lihat di atas).
        local lvl = tonumber(args)
        if not lvl then
            local cur = romp.getPlayerWantedLevel(id) or 0
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                string.format(
                    "*** Wanted level kamu: %d. Pakai: /wanted <0-6>.",
                    cur))
            return true
        end
        if romp.setPlayerWantedLevel(id, lvl) then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                string.format("*** Wanted level kamu diset ke %d.", lvl))
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Gagal: level harus 0-6.")
        end
        return true
    elseif cmd == "wantedof" then
        -- Lihat wanted level pemain lain (GetPlayerWantedLevel):
        -- /wantedof <id>.
        local tp = tonumber(args)
        local lvl = tp and romp.getPlayerWantedLevel(tp)
        if lvl then
            local tname = romp.getPlayerName(tp) or ("id " .. tp)
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                string.format("*** Wanted level %s: %d.", tname, lvl))
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Gagal: pid tak valid.")
        end
        return true
    elseif cmd == "team" then
        -- Demo tim pemain ala SA-MP (SetPlayerTeam):
        -- /team <0-254|255> -> set tim sendiri; /team saja -> lihat tim.
        -- Broadcast 0x53 ke semua pemain + event onPlayerTeamChange
        -- (lihat di atas). 255 = TEAM_NONE (tanpa tim ala SA-MP NO_TEAM).
        local t = tonumber(args)
        if t == nil then
            local cur = romp.getPlayerTeam(id) or romp.TEAM_NONE
            local txt = cur == romp.TEAM_NONE
                and "tanpa tim (TEAM_NONE)"
                or ("tim " .. cur)
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Tim kamu: " .. txt .. ". Pakai: /team <0-254|255>.")
            return true
        end
        if romp.setPlayerTeam(id, t) then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                string.format("*** Tim kamu diset ke %d.", t))
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Gagal: tim harus 0-255.")
        end
        return true
    elseif cmd == "teamof" then
        -- Lihat tim pemain lain (GetPlayerTeam): /teamof <id>.
        local tp = tonumber(args)
        local t = tp and romp.getPlayerTeam(tp)
        if t ~= nil then
            local tname = romp.getPlayerName(tp) or ("id " .. tp)
            local txt = t == romp.TEAM_NONE and "tanpa tim" or ("tim " .. t)
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Tim " .. tname .. ": " .. txt .. ".")
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Gagal: pid tak valid.")
        end
        return true
    elseif cmd == "ptime" then
        -- Demo override jam per-pemain ala SA-MP (SetPlayerTime):
        -- /ptime <jam> <menit> -> override jam sendiri (0x39 efektif
        -- hanya ke kamu); /ptime saja -> lihat jam efektif kamu.
        local h, m = args:match("^(%d+)%s+(%d+)$")
        if h == nil then
            local eh, em, ov = romp.getPlayerTime(id)
            if eh ~= nil then
                romp.sendChatToPlayer(id, romp.KIND_INFO,
                    string.format("*** Jam kamu: %02d:%02d%s. Pakai: /ptime <jam> <menit>.",
                        eh, em, ov and " (override)" or " (global)"))
            else
                romp.sendChatToPlayer(id, romp.KIND_INFO,
                    "*** Gagal membaca jam.")
            end
            return true
        end
        if romp.setPlayerTime(id, tonumber(h), tonumber(m)) then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                string.format("*** Jam kamu dioverride ke %02d:%02d.",
                    tonumber(h), tonumber(m)))
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Gagal: jam 0-23, menit 0-59.")
        end
        return true
    elseif cmd == "ptimeof" then
        -- Lihat jam efektif pemain lain: /ptimeof <id>.
        local tp = tonumber(args)
        local eh, em, ov
        if tp then eh, em, ov = romp.getPlayerTime(tp) end
        if eh ~= nil then
            local tname = romp.getPlayerName(tp) or ("id " .. tp)
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                string.format("*** Jam %s: %02d:%02d%s.",
                    tname, eh, em, ov and " (override)" or " (global)"))
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Gagal: pid tak valid.")
        end
        return true
    elseif cmd == "pweather" then
        -- Demo override cuaca per-pemain ala SA-MP (SetPlayerWeather):
        -- /pweather <id cuaca> -> override cuaca sendiri (0x39 efektif
        -- hanya ke kamu); /pweather saja -> lihat cuaca efektif kamu.
        local w = tonumber(args)
        if w == nil then
            local ew, ov = romp.getPlayerWeather(id)
            if ew ~= nil then
                romp.sendChatToPlayer(id, romp.KIND_INFO,
                    string.format("*** Cuaca kamu: %d%s. Pakai: /pweather <id cuaca>.",
                        ew, ov and " (override)" or " (global)"))
            else
                romp.sendChatToPlayer(id, romp.KIND_INFO,
                    "*** Gagal membaca cuaca.")
            end
            return true
        end
        if romp.setPlayerWeather(id, w) then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                string.format("*** Cuaca kamu dioverride ke %d.", w))
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Gagal: id cuaca 0-255.")
        end
        return true
    elseif cmd == "pweatherof" then
        -- Lihat cuaca efektif pemain lain: /pweatherof <id>.
        local tp = tonumber(args)
        local ew, ov
        if tp then ew, ov = romp.getPlayerWeather(tp) end
        if ew ~= nil then
            local tname = romp.getPlayerName(tp) or ("id " .. tp)
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                string.format("*** Cuaca %s: %d%s.",
                    tname, ew, ov and " (override)" or " (global)"))
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Gagal: pid tak valid.")
        end
        return true
    elseif cmd == "pclock" then
        -- Demo toggle jam ala SA-MP (TogglePlayerClock):
        -- /pclock off -> bekukan jam sendiri (0x54 ke kamu);
        -- /pclock on -> jalankan lagi; /pclock saja -> status.
        if args == "" then
            local st = romp.isPlayerClockEnabled(id)
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Jam kamu: " .. (st == nil and "tak diketahui"
                    or (st and "berjalan" or "dibekukan"))
                .. ". Pakai: /pclock [on|off].")
            return true
        end
        local on = args ~= "off"
        if romp.togglePlayerClock(id, on and 1 or 0) then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Jam kamu " .. (on and "berjalan kembali." or "dibekukan."))
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO, "*** Gagal.")
        end
        return true
    elseif cmd == "uang" then
        -- Demo uang pemain otoritatif ala SA-MP (GetPlayerMoney):
        -- /uang -> cek saldo sendiri (saldo tampil di HUD via 0x49).
        local saldo = romp.getPlayerMoney(id)
        romp.sendChatToPlayer(id, romp.KIND_INFO,
            string.format("*** Saldo kamu: %s.", rp(saldo or 0)))
        return true
    elseif cmd == "setuang" then
        -- Demo uang pemain otoritatif ala SA-MP (SetPlayerMoney):
        -- /setuang <id> <jumlah> -> set saldo pemain (demo, semua boleh).
        local tp, amt = args:match("(%d+)%s+([%-]?%d+)")
        tp, amt = tonumber(tp), tonumber(amt)
        if not tp or not amt then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Pakai: /setuang <id> <jumlah>.")
            return true
        end
        if romp.setPlayerMoney(tp, amt) then
            local tname = romp.getPlayerName(tp) or ("id " .. tp)
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                string.format("*** Saldo %s diset ke %s.",
                    tname, rp(amt)))
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Gagal: id tak valid atau jumlah di luar rentang.")
        end
        return true
    elseif cmd == "kasih" then
        -- Demo uang pemain otoritatif ala SA-MP (GivePlayerMoney):
        -- /kasih <id> <jumlah> -> transfer dari saldo sendiri ke pemain
        -- lain (anti-dupe: transfer hanya via server, saldo diverifikasi).
        local tp, amt = args:match("(%d+)%s+(%d+)")
        tp, amt = tonumber(tp), tonumber(amt)
        if not tp or not amt or amt <= 0 then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Pakai: /kasih <id> <jumlah> (jumlah > 0).")
            return true
        end
        local mine = romp.getPlayerMoney(id) or 0
        if mine < amt then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                string.format("*** Saldo tak cukup (%s).", rp(mine)))
            return true
        end
        local tname = romp.getPlayerName(tp)
        if not tname then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Gagal: id pemain tak valid.")
            return true
        end
        romp.givePlayerMoney(id, -amt)
        romp.givePlayerMoney(tp, amt)
        romp.sendChatToPlayer(id, romp.KIND_INFO,
            string.format("*** Kamu memberi %s kepada %s.", rp(amt), tname))
        romp.sendChatToPlayer(tp, romp.KIND_INFO,
            string.format("*** %s memberimu %s.", romp.getPlayerName(id), rp(amt)))
        return true
    elseif cmd == "modif" then
        -- Komponen kendaraan ala SA-MP (AddVehicleComponent/paintjob):
        -- /modif -> buat Infernus (411) di depan pemain + pasang spoiler
        -- (1000) & velg (1073) + paintjob 1; /modifdel -> hapus.
        local x, y, z = romp.getPlayerPos(id)
        if not x then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Posisi kamu belum diketahui server.")
            return true
        end
        if demoVeh[id] then romp.destroyVehicle(demoVeh[id]) end
        local vid = romp.createVehicle(411, x + 3.0, y, z, 90.0, 6, 6, 0)
        if not vid then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Gagal membuat kendaraan.")
            return true
        end
        demoVeh[id] = vid
        romp.addVehicleComponent(vid, 1000)   -- Pro spoiler (slot 0)
        romp.addVehicleComponent(vid, 1073)   -- Arch Angels wheel (slot 7)
        romp.changeVehiclePaintjob(vid, 1)
        local slot0 = romp.getVehicleComponentInSlot(vid, 0)
        local pj = romp.getVehiclePaintjob(vid)
        romp.sendChatToPlayer(id, romp.KIND_INFO,
            "*** Infernus dimodif (id " .. vid .. "): spoiler=" ..
            tostring(slot0) .. ", paintjob=" .. tostring(pj) ..
            ". /modifdel = hapus.")
        return true
    elseif cmd == "modifdel" then
        -- Hapus kendaraan modif demo milik pemain.
        local vid = demoVeh[id]
        if vid and romp.destroyVehicle(vid) then
            demoVeh[id] = nil
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Kendaraan modif dihapus.")
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Kamu belum punya kendaraan modif (/modif dulu).")
        end
        return true
    elseif cmd == "plat" then
        -- Plat nomor ala SA-MP (SetVehicleNumberPlate): /plat <teks> -> set
        -- plat demoVeh (maks 32 char, dipotong server bila lebih panjang);
        -- /plat tanpa argumen -> hapus plat.
        local vid = demoVeh[id]
        if not vid then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Kamu belum punya kendaraan modif (/modif dulu).")
            return true
        end
        local plate = args:match("^%s*(.-)%s*$")
        if romp.setVehicleNumberPlate(vid, plate) then
            local cur = romp.getVehicleNumberPlate(vid)
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Plat kendaraan id " .. vid .. " kini: \"" .. cur .. "\".")
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Gagal mengganti plat nomor.")
        end
        return true
    elseif cmd == "vparams" then
        -- Params kendaraan ala SA-MP (SetVehicleParamsEx/ForPlayer):
        -- /vparams <field> <0|1> -> set param demoVeh (field: engine|lights|
        -- alarm|doors|bonnet|boot|objective); /vparams -> tampilkan params.
        -- /vparamsfor <objective> <doorslocked> -> params per-pemain.
        local vid = demoVeh[id]
        if not vid then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Kamu belum punya kendaraan modif (/modif dulu).")
            return true
        end
        local fields = { engine = 1, lights = 1, alarm = 1, doors = 1,
            bonnet = 1, boot = 1, objective = 1 }
        local f, v = args:match("^(%a+)%s+(%-?%d+)$")
        if f and fields[f] then
            local vals = { -1, -1, -1, -1, -1, -1, -1 }
            local order = { "engine", "lights", "alarm", "doors",
                "bonnet", "boot", "objective" }
            for i, name in ipairs(order) do
                if name == f then vals[i] = tonumber(v) end
            end
            local ok = romp.setVehicleParamsEx(vid, vals[1], vals[2], vals[3],
                vals[4], vals[5], vals[6], vals[7])
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** setVehicleParamsEx(" .. f .. "=" .. v .. "): " ..
                tostring(ok))
        else
            local p = romp.getVehicleParamsEx(vid)
            if p then
                romp.sendChatToPlayer(id, romp.KIND_INFO,
                    "*** params: engine=" .. p.engine .. " lights=" ..
                    p.lights .. " alarm=" .. p.alarm .. " doors=" .. p.doors ..
                    " bonnet=" .. p.bonnet .. " boot=" .. p.boot ..
                    " objective=" .. p.objective)
            else
                romp.sendChatToPlayer(id, romp.KIND_INFO,
                    "*** getVehicleParamsEx: nil")
            end
        end
        return true
    elseif cmd == "vparamsfor" then
        -- /vparamsfor <objective> <doorslocked> -> params per-pemain
        -- ala SA-MP SetVehicleParamsForPlayer pada demoVeh.
        local vid = demoVeh[id]
        if not vid then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Kamu belum punya kendaraan modif (/modif dulu).")
            return true
        end
        local o, d = args:match("^(%-?%d+)%s+(%-?%d+)$")
        if o then
            local ok = romp.setVehicleParamsForPlayer(vid, id,
                tonumber(o), tonumber(d))
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** setVehicleParamsForPlayer(objective=" .. o ..
                ", doorslocked=" .. d .. "): " .. tostring(ok))
        else
            local p = romp.getVehicleParamsForPlayer(vid, id)
            if p then
                romp.sendChatToPlayer(id, romp.KIND_INFO,
                    "*** params per-pemain: objective=" .. p.objective ..
                    " doorslocked=" .. p.doorslocked)
            else
                romp.sendChatToPlayer(id, romp.KIND_INFO,
                    "*** getVehicleParamsForPlayer: nil")
            end
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
    elseif cmd == "spec" then
        -- Spectate pemain ala SA-MP (PlayerSpectatePlayer):
        -- /spec <id> [mode] -> kamera mengikuti pemain target
        -- (mode 0=normal, 1=fixed, 2=side); /spec tanpa argumen = info.
        local tgt, m = args:match("(%d+)%s*(%d*)")
        tgt = tonumber(tgt)
        if not tgt then
            local s = romp.getPlayerSpectateTarget(id)
            romp.sendChatToPlayer(id, romp.KIND_INFO, s and
                ("*** Spectate: " .. s.type .. " id " .. s.id ..
                 " mode " .. s.mode .. ". /unspec untuk berhenti.") or
                "*** Kamu tidak spectate. Pakai: /spec <id> [mode]")
            return true
        end
        m = (m ~= "" and tonumber(m)) or 0
        if romp.playerSpectatePlayer(id, tgt, m) then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** 👁 Spectate pemain id " .. tgt .. " (mode " .. m .. ").")
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Gagal spectate (target tak valid / beda world).")
        end
        return true
    elseif cmd == "specv" then
        -- Spectate kendaraan ala SA-MP (PlayerSpectateVehicle):
        -- /specv <vid> [mode] -> kamera mengikuti kendaraan.
        local vid, m = args:match("(%d+)%s*(%d*)")
        vid = tonumber(vid)
        m = (m ~= "" and tonumber(m)) or 0
        if vid and romp.playerSpectateVehicle(id, vid, m) then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** 👁 Spectate kendaraan id " .. vid .. ".")
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Gagal spectate kendaraan. Pakai: /specv <vid> [mode]")
        end
        return true
    elseif cmd == "unspec" then
        -- Keluar mode spectate ala SA-MP (TogglePlayerSpectating false).
        if romp.isPlayerSpectating(id) then
            romp.togglePlayerSpectating(id, false)
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** 👁 Mode spectate berhenti.")
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Kamu tidak sedang spectate.")
        end
        return true
    elseif cmd == "keys" then
        -- Lihat state tombol terakhir ala SA-MP (romp.getPlayerKeys):
        -- /keys [id] -> tampilkan bitmask + nama tombol yang ditekan.
        local tgt = tonumber(args:match("(%d+)")) or id
        local k = romp.getPlayerKeys(tgt)
        if k == nil then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Pemain id " .. tgt .. " tidak valid.")
        else
            local who = tgt == id and "kamu" or
                (romp.getPlayerName(tgt) or ("id " .. tgt))
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** ⌨ Tombol " .. who .. ": 0x" ..
                string.format("%04X", k) .. " (" .. key_names(k) .. ")")
        end
        return true
    elseif cmd == "anim" then
        -- Terapkan animasi ala SA-MP (romp.applyAnimationForPlayer):
        -- /anim <lib> <name> [loop] — mis. /anim DANCING dnce_m_a 1.
        -- Tanpa argumen: info animasi aktif (romp.getPlayerAnimation).
        local lib, name, loop = args:match("(%S+)%s+(%S+)%s*(%d*)")
        if not lib or not name then
            local a = romp.getPlayerAnimation(id)
            if a then
                romp.sendChatToPlayer(id, romp.KIND_INFO,
                    "*** 💃 Animasi aktif: " .. a.animlib .. " " .. a.animname ..
                    " (loop=" .. tostring(a.loop) .. ", time=" .. a.time .. ")")
            else
                romp.sendChatToPlayer(id, romp.KIND_INFO,
                    "*** Pakai: /anim <lib> <name> [loop]. Contoh: /anim DANCING dnce_m_a 1")
            end
        else
            local ok = romp.applyAnimationForPlayer(id, lib, name,
                tonumber(loop) or 0)
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                ok and ("*** 💃 Animasi " .. lib .. " " .. name ..
                    " diterapkan ke kamu.")
                or "*** Animasi gagal (lib/name 1..32 char).")
        end
        return true
    elseif cmd == "clearanim" then
        -- Hentikan animasi ala SA-MP ClearAnimations.
        if romp.clearAnimationsForPlayer(id) then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** 💃 Animasi dihentikan.")
        else
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Kamu tidak sedang beranimasi.")
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
