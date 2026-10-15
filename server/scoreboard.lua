-- ============================================================
-- RO-MP Scoreboard ala SA-MP (TAB): daftar pemain + ping.
--
-- Dimuat OTOMATIS oleh server (lua_init) dari folder yang sama dengan
-- file gamemode, persis seperti dialog.lua & textdraw.lua & pickup.lua
-- & vehicle.lua. Pengukuran ping dilakukan server via probe P_PING
-- (0x30) tiap 5 detik; RTT dihitung saat P_PONG (0x31) diterima.
--
-- API:
--   romp.toggleScoreboard(pid, on)  -- true = tampilkan, false = sembunyikan
--   romp.toggleScoreboardAll(on)    -- untuk semua pemain
--   romp.isScoreboardVisible(pid) -> bool
--   romp.getPlayerPing(pid) -> int  -- ping ms, -1 = belum terukur
--
-- Halaman UI: android/.../assets/romp/nui/scoreboard.html
-- ============================================================

local src = debug.getinfo(1, "S").source:match("@?(.*/)")
local json = dofile((src or "./") .. "json.lua")

-- Pemain yang scoreboard-nya sedang tampil: [pid] = key snapshot terakhir
local visible = {}

local function send(pid, msg)
    return romp.sendNUIMessage(pid, json.encode(msg))
end

-- Bangun baris pemain: { id, name, ping, npc }
local function build_rows()
    local rows = {}
    for _, pl in ipairs(romp.getPlayers()) do
        rows[#rows + 1] = { id = pl.id, name = pl.name,
            ping = pl.ping or -1, npc = pl.isnpc and true or false }
    end
    -- Urut ala SA-MP: NPC (bot) tetap di daftar, urut naik id.
    table.sort(rows, function(a, b) return a.id < b.id end)
    return rows
end

local function key_of(rows)
    local parts = {}
    for _, r in ipairs(rows) do
        parts[#parts + 1] = string.format("%d:%s:%d:%s", r.id, r.name, r.ping,
            r.npc and "1" or "0")
    end
    return table.concat(parts, "|")
end

local function push_scoreboard(pid)
    local info = romp.getServerInfo() or {}
    local rows = build_rows()
    local key = key_of(rows)
    local first = visible[pid] == nil
    if not first and visible[pid] == key then
        return -- tak ada perubahan: jangan spam NUI
    end
    visible[pid] = key
    send(pid, { action = first and "showScoreboard" or "updateScoreboard",
        hostname = info.hostname or "RO-MP Server",
        maxplayers = info.maxplayers or 0,
        rows = rows })
end

local function tick_sb()
    for pid in pairs(visible) do
        -- Pastikan pemain masih online sebelum push.
        local alive = false
        for _, pl in ipairs(romp.getPlayers()) do
            if pl.id == pid then alive = true break end
        end
        if alive then
            push_scoreboard(pid)
        else
            visible[pid] = nil
        end
    end
end

-- --- API publik -------------------------------------------------

function romp.toggleScoreboard(pid, on)
    if on then
        if visible[pid] ~= nil then return true end
        visible[pid] = nil -- marker: belum punya snapshot -> showScoreboard
        -- Kirim langsung sekali agar responsif, tick berikutnya lanjut diff.
        local info = romp.getServerInfo() or {}
        local rows = build_rows()
        visible[pid] = key_of(rows)
        return send(pid, { action = "showScoreboard",
            hostname = info.hostname or "RO-MP Server",
            maxplayers = info.maxplayers or 0,
            rows = rows })
    else
        if visible[pid] == nil then return true end
        visible[pid] = nil
        return send(pid, { action = "hideScoreboard" })
    end
end

function romp.toggleScoreboardAll(on)
    for _, pl in ipairs(romp.getPlayers()) do
        romp.toggleScoreboard(pl.id, on)
    end
end

function romp.isScoreboardVisible(pid)
    return visible[pid] ~= nil
end

-- --- Bungkus handler gamemode (file ini dimuat SETELAH gamemode) ---

local _oldTick = _G.onTick
function _G.onTick()
    tick_sb()
    if _oldTick then _oldTick() end
end

local _oldLeave = _G.onPlayerLeave
function _G.onPlayerLeave(id, name, reason)
    visible[id] = nil
    if _oldLeave then return _oldLeave(id, name, reason) end
end
