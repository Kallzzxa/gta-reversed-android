-- ============================================================
-- RO-MP Textdraw helper ala SA-MP — teks 2D di layar via NUI.
--
-- Dimuat OTOMATIS oleh server (lua_init) dari folder yang sama dengan
-- file gamemode, persis seperti dialog.lua. Tidak butuh callback dari
-- client; semua lewat romp.sendNUIMessage (0x40 NUI_MESSAGE).
--
-- API global (ala SA-MP TextDraw*):
--   id = romp.createTextDraw(x, y, text)      -- x,y koordinat 0-640 x 0-480
--   romp.destroyTextDraw(id) -> bool
--   romp.showTextDraw(id) -> bool             -- tampil untuk SEMUA pemain
--   romp.hideTextDraw(id) -> bool
--   romp.showTextDrawForPlayer(pid, id) -> bool
--   romp.hideTextDrawForPlayer(pid, id) -> bool
--   romp.setTextDrawString(id, text) -> bool  -- ubah teks yang tampil
--   romp.setTextDrawStyle(id, opts) -> bool   -- lihat STYLE di bawah
--   romp.isTextDrawVisible(id) -> bool
--
-- API per-pemain (ala SA-MP PlayerTextDraw*):
--   id = romp.createPlayerTextDraw(pid, x, y, text)
--   romp.destroyPlayerTextDraw(pid, id) -> bool
--   romp.showPlayerTextDraw(pid, id) -> bool
--   romp.hidePlayerTextDraw(pid, id) -> bool
--   romp.setPlayerTextDrawString(pid, id, text) -> bool
--   romp.setPlayerTextDrawStyle(pid, id, opts) -> bool
--
-- STYLE (opts table, semua opsional):
--   color    = 0xRRGGBBAA (default 0xFFFFFFFF)
--   size     = pengali ukuran huruf (default 1.0)
--   align    = 1 kiri, 2 tengah, 3 kanan (default 1)
--   font     = 0..3 (default 1)
--   box      = 0/1 tampilkan kotak latar (default 0)
--   boxColor = 0xRRGGBBAA (default 0x00000088)
--   outline  = 0/1 garis tepi teks (default 1)
--   shadow   = 0/1 bayangan teks (default 0)
--
-- Halaman UI: android/.../assets/romp/nui/textdraw.html
-- ============================================================

-- Cari json.lua di folder yang sama dengan file ini (tidak tergantung CWD).
local src = debug.getinfo(1, "S").source:match("@?(.*/)")
local json = dofile((src or "./") .. "json.lua")

local MAX_GLOBAL, MAX_PLAYER = 256, 64

local globals   = {}  -- [id] = {x,y,text,style,visibleAll,viewers={pid=true}}
local perplayer = {}  -- [pid] = { [id] = {x,y,text,style,visible} }
local gFree, nextGid = {}, 1

local DEF_STYLE = {
    color = 0xFFFFFFFF, size = 1.0, align = 1, font = 1,
    box = 0, boxColor = 0x00000088, outline = 1, shadow = 0,
}

local function norm_style(opts)
    local s = {}
    for k, v in pairs(DEF_STYLE) do s[k] = v end
    if type(opts) == "table" then
        for k, v in pairs(opts) do
            if DEF_STYLE[k] ~= nil and type(v) == "number" then s[k] = v end
        end
    end
    return s
end

local function msg_show(key, td)
    return json.encode({
        action = "showTextDraw", id = key, text = td.text,
        x = td.x, y = td.y, color = td.style.color, size = td.style.size,
        align = td.style.align, font = td.style.font, box = td.style.box,
        boxColor = td.style.boxColor, outline = td.style.outline,
        shadow = td.style.shadow,
    })
end

local function send(pid, payload)
    return romp.sendNUIMessage(pid, payload)
end

local function valid_td(t)
    return type(t) == "table" and type(t.x) == "number"
        and type(t.y) == "number" and type(t.text) == "string"
end

-- ---------------- Global ----------------

function romp.createTextDraw(x, y, text)
    if type(x) ~= "number" or type(y) ~= "number"
        or type(text) ~= "string" then return nil end
    local id = table.remove(gFree) or nextGid
    if id > MAX_GLOBAL then return nil end
    if id == nextGid then nextGid = nextGid + 1 end
    globals[id] = { x = x, y = y, text = text,
                    style = norm_style(nil), visibleAll = false, viewers = {} }
    return id
end

function romp.destroyTextDraw(id)
    local td = globals[id]
    if not valid_td(td) then return false end
    local payload = json.encode({ action = "hideTextDraw", id = "g" .. id })
    if td.visibleAll then
        for _, p in ipairs(romp.getPlayers()) do send(p.id, payload) end
    else
        for pid in pairs(td.viewers) do send(pid, payload) end
    end
    globals[id] = nil
    table.insert(gFree, id)
    return true
end

function romp.showTextDraw(id)
    local td = globals[id]
    if not valid_td(td) then return false end
    local payload = msg_show("g" .. id, td)
    local n = 0
    for _, p in ipairs(romp.getPlayers()) do
        if send(p.id, payload) then n = n + 1 end
    end
    td.visibleAll = true
    td.viewers = {}
    return n > 0
end

function romp.hideTextDraw(id)
    local td = globals[id]
    if not valid_td(td) then return false end
    local payload = json.encode({ action = "hideTextDraw", id = "g" .. id })
    for _, p in ipairs(romp.getPlayers()) do send(p.id, payload) end
    td.visibleAll = false
    td.viewers = {}
    return true
end

function romp.showTextDrawForPlayer(pid, id)
    local td = globals[id]
    if not valid_td(td) then return false end
    if not send(pid, msg_show("g" .. id, td)) then return false end
    td.viewers[pid] = true
    return true
end

function romp.hideTextDrawForPlayer(pid, id)
    local td = globals[id]
    if not valid_td(td) then return false end
    send(pid, json.encode({ action = "hideTextDraw", id = "g" .. id }))
    td.viewers[pid] = nil
    return true
end

local function restyle_global(id, td)
    local payload = msg_show("g" .. id, td)
    if td.visibleAll then
        for _, p in ipairs(romp.getPlayers()) do send(p.id, payload) end
    else
        for pid in pairs(td.viewers) do send(pid, payload) end
    end
end

function romp.setTextDrawString(id, text)
    local td = globals[id]
    if not valid_td(td) or type(text) ~= "string" then return false end
    td.text = text
    local payload = json.encode(
        { action = "setTextDrawString", id = "g" .. id, text = text })
    if td.visibleAll then
        for _, p in ipairs(romp.getPlayers()) do send(p.id, payload) end
    else
        for pid in pairs(td.viewers) do send(pid, payload) end
    end
    return true
end

function romp.setTextDrawStyle(id, opts)
    local td = globals[id]
    if not valid_td(td) then return false end
    td.style = norm_style(opts)
    restyle_global(id, td)
    return true
end

function romp.isTextDrawVisible(id)
    local td = globals[id]
    if not valid_td(td) then return false end
    return td.visibleAll or next(td.viewers) ~= nil
end

-- ---------------- Per-pemain ----------------

local function ptds(pid)
    local t = perplayer[pid]
    if not t then t = {}; perplayer[pid] = t end
    return t
end

local function pkey(pid, id) return "p" .. pid .. "_" .. id end

function romp.createPlayerTextDraw(pid, x, y, text)
    if type(x) ~= "number" or type(y) ~= "number"
        or type(text) ~= "string" then return nil end
    local t = ptds(pid)
    local id = nil
    for i = 1, MAX_PLAYER do
        if not t[i] then id = i; break end
    end
    if not id then return nil end
    t[id] = { x = x, y = y, text = text,
              style = norm_style(nil), visible = false }
    return id
end

function romp.destroyPlayerTextDraw(pid, id)
    local t = perplayer[pid]
    local td = t and t[id]
    if not valid_td(td) then return false end
    send(pid, json.encode({ action = "hideTextDraw", id = pkey(pid, id) }))
    t[id] = nil
    return true
end

function romp.showPlayerTextDraw(pid, id)
    local t = perplayer[pid]
    local td = t and t[id]
    if not valid_td(td) then return false end
    if not send(pid, msg_show(pkey(pid, id), td)) then return false end
    td.visible = true
    return true
end

function romp.hidePlayerTextDraw(pid, id)
    local t = perplayer[pid]
    local td = t and t[id]
    if not valid_td(td) then return false end
    send(pid, json.encode({ action = "hideTextDraw", id = pkey(pid, id) }))
    td.visible = false
    return true
end

function romp.setPlayerTextDrawString(pid, id, text)
    local t = perplayer[pid]
    local td = t and t[id]
    if not valid_td(td) or type(text) ~= "string" then return false end
    td.text = text
    if td.visible then
        send(pid, json.encode(
            { action = "setTextDrawString", id = pkey(pid, id), text = text }))
    end
    return true
end

function romp.setPlayerTextDrawStyle(pid, id, opts)
    local t = perplayer[pid]
    local td = t and t[id]
    if not valid_td(td) then return false end
    td.style = norm_style(opts)
    if td.visible then send(pid, msg_show(pkey(pid, id), td)) end
    return true
end

-- ---------------- Sinkronisasi join/leave ----------------
-- Bungkus handler gamemode (file ini dimuat SETELAH gamemode).

local _oldJoin = _G.onPlayerJoin
function _G.onPlayerJoin(id, name)
    -- Pemain baru langsung dapat textdraw global yang sedang tampil.
    for tdid, td in pairs(globals) do
        if td.visibleAll then send(id, msg_show("g" .. tdid, td)) end
    end
    if _oldJoin then return _oldJoin(id, name) end
end

local _oldLeave = _G.onPlayerLeave
function _G.onPlayerLeave(id, name)
    for _, td in pairs(globals) do td.viewers[id] = nil end
    perplayer[id] = nil
    if _oldLeave then return _oldLeave(id, name) end
end
