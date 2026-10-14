-- ============================================================
-- RO-MP Map icon / radar blip ala SA-MP via Lua + NUI.
--
-- Dimuat OTOMATIS oleh server (lua_init) dari folder yang sama dengan
-- file gamemode, persis seperti dialog.lua & textdraw.lua. Posisi pemain
-- dikirim ke NUI tiap detik (event onTick, dibungkus) agar radar di
-- halaman UI bisa menggambar blip relatif terhadap pemain.
--
-- API global (mirip SA-MP SetPlayerMapIcon / RemovePlayerMapIcon):
--   ok = romp.setPlayerMapIcon(pid, iconid, x, y, z, markertype, color, style)
--        iconid: 0..99 per pemain (ala SA-MP; set ulang menimpa yang lama)
--        markertype: 0=noinit, 1=local, 2=global,
--                    3=local checkpoint, 4=global checkpoint
--        color: 0xRRGGBBAA (mis. 0xFF0000FF = merah)
--        style: 0=noinit, 1=permanen, 2=berkedip
--   ok = romp.removePlayerMapIcon(pid, iconid)
--   n  = romp.removeAllPlayerMapIcons(pid) -> jumlah icon yang dihapus
--   t  = romp.getPlayerMapIcon(pid, iconid)
--        -> {iconid, x, y, z, markertype, color, style} | nil
--   romp.MAPICON_LOCAL / MAPICON_GLOBAL /
--   romp.MAPICON_LOCAL_CHECKPOINT / romp.MAPICON_GLOBAL_CHECKPOINT
--   romp.MAPICON_STYLE_PERMANENT / romp.MAPICON_STYLE_FLASHING
--
-- Pesan NUI 0x40 (id "mi<iconid>"):
--   {"action":"setMapIcon","id":"mi5","iconid":5,
--    "x":100.0,"y":200.0,"z":3.0,"markertype":2,
--    "color":4278190335,"style":1}
--   {"action":"removeMapIcon","id":"mi5"}
--   {"action":"playerPos","x":98.0,"y":199.0,"z":3.0}
--
-- Halaman UI: android/.../assets/romp/nui/mapicon.html
-- ============================================================

local src = debug.getinfo(1, "S").source:match("@?(.*/)")
local json = dofile((src or "./") .. "json.lua")

romp.MAPICON_LOCAL            = 1
romp.MAPICON_GLOBAL           = 2
romp.MAPICON_LOCAL_CHECKPOINT = 3
romp.MAPICON_GLOBAL_CHECKPOINT = 4

romp.MAPICON_STYLE_PERMANENT = 1
romp.MAPICON_STYLE_FLASHING  = 2

local MAX_ICONID = 99

local icons = {}  -- [pid] = { [iconid] = {x,y,z,markertype,color,style} }

local function num(v, d)
    return type(v) == "number" and v or d
end

local function valid_icon(iconid, x, y, z, markertype, color, style)
    if type(iconid) ~= "number" or iconid < 0 or iconid > MAX_ICONID
        or math.floor(iconid) ~= iconid then return false end
    if type(x) ~= "number" or type(y) ~= "number"
        or type(z) ~= "number" then return false end
    if markertype ~= 0 and markertype ~= 1 and markertype ~= 2
        and markertype ~= 3 and markertype ~= 4 then return false end
    if type(color) ~= "number" or color < 0 or color > 0xFFFFFFFF
        or math.floor(color) ~= color then return false end
    if style ~= 0 and style ~= 1 and style ~= 2 then return false end
    return true
end

local function msg_set(iconid, ic)
    return json.encode({
        action = "setMapIcon", id = "mi" .. iconid, iconid = iconid,
        x = ic.x, y = ic.y, z = ic.z,
        markertype = ic.markertype, color = ic.color, style = ic.style,
    })
end

local function send(pid, payload)
    return romp.sendNUIMessage(pid, payload)
end

local function player_icons(pid, create)
    local t = icons[pid]
    if not t and create then
        t = {}
        icons[pid] = t
    end
    return t
end

function romp.setPlayerMapIcon(pid, iconid, x, y, z, markertype, color, style)
    markertype = num(markertype, 1)
    color      = num(color, 0xFFFFFFFF)
    style      = num(style, 1)
    if not valid_icon(iconid, x, y, z, markertype, color, style) then
        return false
    end
    local t = player_icons(pid, true)
    t[iconid] = { x = x, y = y, z = z,
                  markertype = markertype, color = color, style = style }
    return send(pid, msg_set(iconid, t[iconid])) and true or false
end

function romp.removePlayerMapIcon(pid, iconid)
    local t = player_icons(pid)
    if not t or not t[iconid] then return false end
    t[iconid] = nil
    send(pid, json.encode({ action = "removeMapIcon", id = "mi" .. iconid }))
    return true
end

function romp.removeAllPlayerMapIcons(pid)
    local t = player_icons(pid)
    if not t then return 0 end
    local n = 0
    for iconid in pairs(t) do
        t[iconid] = nil
        send(pid, json.encode({ action = "removeMapIcon",
                                id = "mi" .. iconid }))
        n = n + 1
    end
    return n
end

function romp.getPlayerMapIcon(pid, iconid)
    local t = player_icons(pid)
    local ic = t and t[iconid]
    if not ic then return nil end
    return { iconid = iconid, x = ic.x, y = ic.y, z = ic.z,
             markertype = ic.markertype, color = ic.color, style = ic.style }
end

-- ---------------- Posisi pemain -> radar (tiap detik via onTick) ----------------

local function tick_pos()
    for pid, t in pairs(icons) do
        local has = false
        for _ in pairs(t) do has = true break end
        if has then
            local x, y, z = romp.getPlayerPos(pid)
            if x then
                send(pid, json.encode({ action = "playerPos",
                                        x = x, y = y, z = z }))
            end
        end
    end
end

-- Bungkus handler gamemode (file ini dimuat SETELAH gamemode).
local _oldTick = _G.onTick
function _G.onTick()
    tick_pos()
    if _oldTick then _oldTick() end
end

local _oldLeave = _G.onPlayerLeave
function _G.onPlayerLeave(id, name)
    icons[id] = nil
    if _oldLeave then return _oldLeave(id, name) end
end
