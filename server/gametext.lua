-- ============================================================
-- RO-MP GameText ala SA-MP — teks besar gaya GTA di layar via NUI.
--
-- Dimuat OTOMATIS oleh server (daftar g_helpers di romp_lua.c) dari
-- folder yang sama dengan file gamemode, persis seperti dialog.lua &
-- textdraw.lua. Murni Lua: tidak ada paket/protokol baru — semua lewat
-- romp.sendNUIMessage (0x40 NUI_MESSAGE) ke halaman
-- android/app/src/main/assets/romp/nui/gametext.html.
--
-- API ala SA-MP:
--   romp.gameTextForPlayer(pid, text, time, style) -> bool
--   romp.gameTextForAll(text, time, style)         -> int (jumlah terkirim)
--   romp.gameTextForPlayers(pids, text, time, style) -> int
--   romp.hideGameTextForPlayer(pid) -> bool   -- sembunyikan lebih awal
--   romp.hideGameTextForAll() -> int
--   romp.getGameText(pid) -> text|nil          -- teks yang sedang tampil
--
--   time  = lama tampil dalam ms (default 3000, dijepit 500..30000)
--   style = 0..6 (default 0; di luar rentang dinormalkan ke 0)
--     0 = besar di tengah, fade in/out (ala tulisan "WASTED")
--     1 = besar di tengah
--     2 = besar di tengah + latar gelap
--     3 = banner kecil di tengah atas
--     4 = banner kecil di tengah bawah
--     5 = di tengah, efek mesin ketik
--     6 = banner tengah atas + latar gelap
--
-- Teks mendukung kode warna ala SA-MP: ~r~ ~g~ ~b~ ~y~ ~p~ ~w~ ~l~ ~h~
-- dan ~n~ untuk baris baru. Panjang maks 256 karakter (dipotong).
-- ============================================================

local src = debug.getinfo(1, "S").source:match("@?(.*/)")
local json = dofile((src or "./") .. "json.lua")

local TEXT_MAX = 256
local TIME_MIN = 500
local TIME_MAX = 30000
local TIME_DEF = 3000

local active = {}  -- [pid] = { text = string, timer = timerid }

local function player_exists(pid)
    for _, p in ipairs(romp.getPlayers() or {}) do
        if p.id == pid then return true end
    end
    return false
end

-- Normalkan argumen; return nil bila teks kosong/tak valid.
local function norm_args(text, time, style)
    if type(text) ~= "string" then text = tostring(text or "") end
    if text == "" then return nil end
    text = text:sub(1, TEXT_MAX)
    time = tonumber(time) or TIME_DEF
    if time < TIME_MIN then time = TIME_MIN end
    if time > TIME_MAX then time = TIME_MAX end
    style = math.floor(tonumber(style) or 0)
    if style < 0 or style > 6 then style = 0 end
    return text, time, style
end

local function clear_state(pid)
    local a = active[pid]
    active[pid] = nil
    if a and a.timer then romp.killTimer(a.timer) end
end

-- ala SA-MP GameTextForPlayer(playerid, text[], time, style).
function romp.gameTextForPlayer(pid, text, time, style)
    pid = tonumber(pid)
    if not pid or not player_exists(pid) then return false end
    text, time, style = norm_args(text, time, style)
    if not text then return false end
    clear_state(pid)
    local ok = romp.sendNUIMessage(pid, json.encode({
        action = "gameText", text = text, time = time, style = style,
    }))
    if not ok then return false end
    -- Catat yang sedang tampil; kedaluwarsa mengikuti `time`
    -- (client juga menyembunyikan sendiri setelah `time` ms).
    local timer = romp.setTimer(function() active[pid] = nil end,
                                time, false)
    active[pid] = { text = text, timer = timer }
    return true
end

-- Kirim ke daftar pemain (tabel berisi id). -> jumlah yang terkirim.
function romp.gameTextForPlayers(pids, text, time, style)
    if type(pids) ~= "table" then return 0 end
    text, time, style = norm_args(text, time, style)
    if not text then return 0 end
    local n = 0
    for _, pid in ipairs(pids) do
        if romp.gameTextForPlayer(pid, text, time, style) then
            n = n + 1
        end
    end
    return n
end

-- ala SA-MP GameTextForAll(text[], time, style). -> jumlah terkirim.
function romp.gameTextForAll(text, time, style)
    local pids = {}
    for _, p in ipairs(romp.getPlayers() or {}) do
        pids[#pids + 1] = p.id
    end
    return romp.gameTextForPlayers(pids, text, time, style)
end

-- Sembunyikan GameText lebih awal (ala SA-MP tidak ada padanannya;
-- di SA-MP teks hilang sendiri setelah `time` habis).
function romp.hideGameTextForPlayer(pid)
    pid = tonumber(pid)
    if not pid or not player_exists(pid) then return false end
    clear_state(pid)
    return romp.sendNUIMessage(pid, json.encode({ action = "hideGameText" }))
end

function romp.hideGameTextForAll()
    local n = 0
    for _, p in ipairs(romp.getPlayers() or {}) do
        if romp.hideGameTextForPlayer(p.id) then n = n + 1 end
    end
    return n
end

-- Teks GameText yang sedang tampil untuk pemain (nil bila tidak ada).
-- Perpanjangan RO-MP (SA-MP tidak punya getter ini).
function romp.getGameText(pid)
    local a = active[tonumber(pid)]
    return a and a.text or nil
end

-- ---------------- Hook event (dibungkus, tidak menimpa gamemode) ----------------

local _oldLeave = _G.onPlayerLeave
function _G.onPlayerLeave(pid, name, reason)
    clear_state(pid)
    if _oldLeave then return _oldLeave(pid, name, reason) end
end
