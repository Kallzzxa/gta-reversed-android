-- ============================================================
-- RO-MP Nametag + Chat Bubble ala SA-MP.
--
-- Dimuat OTOMATIS oleh server (lua_init) dari folder yang sama dengan
-- file gamemode, persis seperti dialog.lua & textdraw.lua & pickup.lua
-- & scoreboard.lua & vehicle.lua.
--
-- Implementasi memakai sistem 3D text label yang sudah ada
-- (romp.create3DTextLabel, paket 0x28-0x2B):
--   * Nametag  = label berisi nama pemain, di-attach ke pemain
--     (offset default di atas kepala). Dibuat saat pemain join,
--     dihapus saat leave (server juga otomatis menghapus label yang
--     di-attach saat pemain disconnect).
--   * Chat bubble = label sementara berisi teks chat, di-attach di
--     atas nametag, otomatis dihapus setelah expiretime
--     (ala SA-MP SetPlayerChatBubble).
--
-- API:
--   romp.showNameTags(on)          -- ala SA-MP ShowNameTags; default true
--   romp.areNameTagsEnabled() -> bool
--   romp.setNameTagDrawDistance(d) -- ala SA-MP SetNameTagDrawDistance (default 25.0)
--   romp.getNameTagDrawDistance() -> number
--   romp.setPlayerNametagColor(pid, color) -- warna nametag (0xRRGGBBAA)
--   romp.setPlayerChatBubble(pid, text, color, drawdistance, expiretime)
--        -- ala SA-MP SetPlayerChatBubble; text kosong/nil = sembunyikan
--   romp.setAutoChatBubble(on)     -- bubble otomatis tiap ada chat; default true
--   romp.isAutoChatBubble() -> bool
-- ============================================================

local WHITE = 0xFFFFFFFF
local NAMETAG_Z = 2.6   -- offset label di atas kepala (default attach pemain)
local BUBBLE_Z  = 3.4   -- bubble di atas nametag
local LABEL_TEXT_MAX = 200

local enabled     = true
local drawdist    = 25.0
local auto_bubble = true

local nametag_of = {}  -- [pid] = labelid
local bubble_of  = {}  -- [pid] = { label = labelid, timer = timerid }
local color_of   = {}  -- [pid] = warna nametag (override)

-- Virtual world pemain (untuk vw label saat dibuat; setelah di-attach
-- vw label otomatis mengikuti pemain).
local function player_vw(pid)
    for _, pl in ipairs(romp.getPlayers()) do
        if pl.id == pid then return pl.vw or 0 end
    end
    return 0
end

local function make_nametag(pid, name)
    if nametag_of[pid] then return nametag_of[pid] end
    if not name or name == "" then return nil end
    local id = romp.create3DTextLabel(name, color_of[pid] or WHITE,
        0, 0, 0, drawdist, player_vw(pid))
    if not id then return nil end
    if not romp.attach3DTextLabelToPlayer(id, pid, 0, 0, NAMETAG_Z) then
        romp.delete3DTextLabel(id)
        return nil
    end
    nametag_of[pid] = id
    return id
end

local function remove_nametag(pid)
    local id = nametag_of[pid]
    nametag_of[pid] = nil
    if id then romp.delete3DTextLabel(id) end
end

local function clear_bubble(pid)
    local b = bubble_of[pid]
    bubble_of[pid] = nil
    if not b then return end
    if b.timer then romp.killTimer(b.timer) end
    romp.delete3DTextLabel(b.label)
end

-- ---------------- API publik ----------------

-- ala SA-MP ShowNameTags. Menyalakan = buat nametag untuk semua pemain
-- yang online; mematikan = hapus semua nametag.
function romp.showNameTags(on)
    on = not not on
    if on == enabled then return true end
    enabled = on
    if on then
        for _, pl in ipairs(romp.getPlayers()) do
            make_nametag(pl.id, pl.name)
        end
    else
        for pid, _ in pairs(nametag_of) do remove_nametag(pid) end
    end
    return true
end

function romp.areNameTagsEnabled() return enabled end

-- ala SA-MP SetNameTagDrawDistance. Label dibuat ulang supaya jarak
-- render yang baru langsung berlaku.
function romp.setNameTagDrawDistance(d)
    d = tonumber(d) or 25.0
    if d <= 0 then d = 25.0 end
    if d > 1000 then d = 1000 end
    drawdist = d
    local names = {}
    for _, pl in ipairs(romp.getPlayers()) do names[pl.id] = pl.name end
    for pid, _ in pairs(nametag_of) do remove_nametag(pid) end
    if enabled then
        for pid, name in pairs(names) do make_nametag(pid, name) end
    end
    return true
end

function romp.getNameTagDrawDistance() return drawdist end

-- Warna nametag satu pemain (0xRRGGBBAA). Di SA-MP ini perannya
-- dipegang SetPlayerColor; di sini eksplisit per nametag.
function romp.setPlayerNametagColor(pid, color)
    color = tonumber(color) or WHITE
    color_of[pid] = color
    local id = nametag_of[pid]
    if not id then return false end
    local name = romp.getPlayerName(pid)
    if not name then return false end
    return romp.update3DTextLabelText(id, color, name)
end

-- ala SA-MP SetPlayerChatBubble(playerID, text, color, drawdistance,
-- expiretime). expiretime dalam ms (50..30000). text kosong = sembunyikan.
-- Satu pemain hanya punya satu bubble aktif; bubble baru menggantikan lama.
function romp.setPlayerChatBubble(pid, text, color, dist, expire)
    clear_bubble(pid)
    if not text or text == "" then return true end
    text = tostring(text):sub(1, LABEL_TEXT_MAX)
    local id = romp.create3DTextLabel(text, tonumber(color) or WHITE,
        0, 0, 0, tonumber(dist) or drawdist, player_vw(pid))
    if not id then return false end
    if not romp.attach3DTextLabelToPlayer(id, pid, 0, 0, BUBBLE_Z) then
        romp.delete3DTextLabel(id)
        return false
    end
    local ms = tonumber(expire) or 5000
    if ms < 50 then ms = 50 end
    if ms > 30000 then ms = 30000 end
    local timer = romp.setTimer(function()
        bubble_of[pid] = nil
        romp.delete3DTextLabel(id)
    end, ms, false)
    bubble_of[pid] = { label = id, timer = timer }
    return true
end

-- Bubble otomatis tiap ada chat normal (ala SA-MP). Default true.
function romp.setAutoChatBubble(on)
    auto_bubble = not not on
    return true
end

function romp.isAutoChatBubble() return auto_bubble end

-- ---------------- Hook event (dibungkus, tidak menimpa gamemode) ----------------

local _oldJoin = _G.onPlayerJoin
function _G.onPlayerJoin(pid, name)
    if enabled then make_nametag(pid, name) end
    if _oldJoin then return _oldJoin(pid, name) end
end

local _oldLeave = _G.onPlayerLeave
function _G.onPlayerLeave(pid, name, reason)
    remove_nametag(pid)
    clear_bubble(pid)
    color_of[pid] = nil
    if _oldLeave then return _oldLeave(pid, name, reason) end
end

-- Chat normal (bukan command) otomatis memunculkan bubble 5 detik,
-- kecuali pesan diblokir handler gamemode (return true).
local _oldChat = _G.onPlayerChat
function _G.onPlayerChat(pid, name, text)
    local blocked = _oldChat and _oldChat(pid, name, text)
    if not blocked and auto_bubble then
        romp.setPlayerChatBubble(pid, text, WHITE, drawdist, 5000)
    end
    return blocked
end
