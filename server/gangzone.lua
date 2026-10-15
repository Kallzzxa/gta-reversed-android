-- ============================================================
-- RO-MP GangZone ala SA-MP.
--
-- Dimuat OTOMATIS oleh server (lua_init) dari folder yang sama dengan
-- file gamemode, persis seperti dialog.lua & textdraw.lua & mapicon.lua
-- & nametag.lua.
--
-- Zona adalah entitas GLOBAL (kotak minx,miny,maxx,maxy), tapi
-- visibilitasnya per-pemain ala SA-MP (GangZoneShowForPlayer /
-- GangZoneShowForAll). Rendering di radar NUI (mapicon.html) sebagai
-- kotak semi-transparan; flash = warna bergantian tiap 0.5 detik.
--
-- API global (mirip SA-MP):
--   zid = romp.createGangZone(minx, miny, maxx, maxy) -> zoneid | nil
--         (maks 1024 zona; min>max otomatis ditukar)
--   ok  = romp.destroyGangZone(zid)
--   ok  = romp.showGangZoneForPlayer(pid, zid, color)
--   ok  = romp.showGangZoneForAll(zid, color)
--   ok  = romp.hideGangZoneForPlayer(pid, zid)
--   ok  = romp.hideGangZoneForAll(zid)
--   ok  = romp.flashGangZoneForPlayer(pid, zid, flashcolor)
--   ok  = romp.flashGangZoneForAll(zid, flashcolor)
--   ok  = romp.stopGangZoneFlashForPlayer(pid, zid)
--   ok  = romp.stopGangZoneFlashForAll(zid)
--   z   = romp.getGangZone(zid) -> {minx,miny,maxx,maxy} | nil
--   t   = romp.getVisibleGangZones(pid) -> { [zid] = {color, flash} }
--
-- Pesan NUI 0x40 (id "gz<zoneid>"):
--   {"action":"setGangZone","id":"gz3","zoneid":3,
--    "minx":100.0,"miny":200.0,"maxx":300.0,"maxy":400.0,
--    "color":4278190335,"flash":0}
--   {"action":"removeGangZone","id":"gz3"}
--   ("flash": 0 = tidak berkedip; selain itu warna flash 0xRRGGBBAA)
--
-- Halaman UI: android/.../assets/romp/nui/mapicon.html (radar).
-- ============================================================

local src = debug.getinfo(1, "S").source:match("@?(.*/)")
local json = dofile((src or "./") .. "json.lua")

local MAX_ZONES = 1024

local zones   = {}  -- [zid] = {minx,miny,maxx,maxy}
local next_zid = 0
local free_zids = {} -- stack id bekas destroy (dipakai ulang)
local visible = {}  -- [pid] = { [zid] = {color=.., flash=..} }

local function num(v, d)
    return type(v) == "number" and v or d
end

local function valid_zone(zid)
    return zones[zid] ~= nil
end

local function msg_set(zid, z, color, flash)
    return json.encode({
        action = "setGangZone", id = "gz" .. zid, zoneid = zid,
        minx = z.minx, miny = z.miny, maxx = z.maxx, maxy = z.maxy,
        color = color, flash = flash or 0,
    })
end

local function send(pid, payload)
    return romp.sendNUIMessage(pid, payload)
end

local function vis_of(pid, create)
    local t = visible[pid]
    if not t and create then
        t = {}
        visible[pid] = t
    end
    return t
end

-- ---------------- API publik ----------------

-- ala SA-MP GangZoneCreate. Mengembalikan zoneid (0..) atau nil bila penuh.
function romp.createGangZone(minx, miny, maxx, maxy)
    if type(minx) ~= "number" or type(miny) ~= "number"
        or type(maxx) ~= "number" or type(maxy) ~= "number" then
        return nil
    end
    local count = 0
    for _ in pairs(zones) do count = count + 1 end
    if count >= MAX_ZONES then return nil end
    if minx > maxx then minx, maxx = maxx, minx end
    if miny > maxy then miny, maxy = maxy, miny end
    local zid
    if #free_zids > 0 then
        zid = table.remove(free_zids)
    else
        zid = next_zid
        next_zid = next_zid + 1
    end
    zones[zid] = { minx = minx, miny = miny, maxx = maxx, maxy = maxy }
    return zid
end

-- ala SA-MP GangZoneDestroy. Zona hilang dari semua pemain yang melihatnya.
function romp.destroyGangZone(zid)
    local z = zones[zid]
    if not z then return false end
    zones[zid] = nil
    free_zids[#free_zids + 1] = zid
    local payload = json.encode({ action = "removeGangZone",
                                  id = "gz" .. zid })
    for pid, t in pairs(visible) do
        if t[zid] then
            t[zid] = nil
            send(pid, payload)
        end
    end
    return true
end

-- ala SA-MP GangZoneShowForPlayer. Menimpa warna/flash yang sudah ada.
function romp.showGangZoneForPlayer(pid, zid, color)
    if not valid_zone(zid) then return false end
    color = num(color, 0xFFFFFFFF)
    local t = vis_of(pid, true)
    local v = t[zid] or {}
    v.color = color
    t[zid] = v
    return send(pid, msg_set(zid, zones[zid], v.color, v.flash or 0))
        and true or false
end

-- ala SA-MP GangZoneShowForAll.
function romp.showGangZoneForAll(zid, color)
    if not valid_zone(zid) then return false end
    local ok = true
    for _, pl in ipairs(romp.getPlayers()) do
        if not romp.showGangZoneForPlayer(pl.id, zid, color) then
            ok = false
        end
    end
    return ok
end

-- ala SA-MP GangZoneHideForPlayer.
function romp.hideGangZoneForPlayer(pid, zid)
    local t = vis_of(pid)
    if not t or not t[zid] then return false end
    t[zid] = nil
    send(pid, json.encode({ action = "removeGangZone",
                            id = "gz" .. zid }))
    return true
end

-- ala SA-MP GangZoneHideForAll.
function romp.hideGangZoneForAll(zid)
    if not valid_zone(zid) then return false end
    local payload = json.encode({ action = "removeGangZone",
                                  id = "gz" .. zid })
    for pid, t in pairs(visible) do
        if t[zid] then
            t[zid] = nil
            send(pid, payload)
        end
    end
    return true
end

-- ala SA-MP GangZoneFlashForPlayer. flashcolor 0 = berhenti (seperti StopFlash).
function romp.flashGangZoneForPlayer(pid, zid, flashcolor)
    if not valid_zone(zid) then return false end
    local t = vis_of(pid, true)
    local v = t[zid] or {}
    v.flash = num(flashcolor, 0)
    t[zid] = v
    return send(pid, msg_set(zid, zones[zid], v.color or 0xFFFFFFFF,
                             v.flash)) and true or false
end

-- ala SA-MP GangZoneFlashForAll.
function romp.flashGangZoneForAll(zid, flashcolor)
    if not valid_zone(zid) then return false end
    local ok = true
    for _, pl in ipairs(romp.getPlayers()) do
        if not romp.flashGangZoneForPlayer(pl.id, zid, flashcolor) then
            ok = false
        end
    end
    return ok
end

-- ala SA-MP GangZoneStopFlashForPlayer.
function romp.stopGangZoneFlashForPlayer(pid, zid)
    return romp.flashGangZoneForPlayer(pid, zid, 0)
end

-- ala SA-MP GangZoneStopFlashForAll.
function romp.stopGangZoneFlashForAll(zid)
    return romp.flashGangZoneForAll(zid, 0)
end

function romp.getGangZone(zid)
    local z = zones[zid]
    if not z then return nil end
    return { minx = z.minx, miny = z.miny, maxx = z.maxx, maxy = z.maxy }
end

-- Daftar zona yang terlihat oleh pemain: { [zid] = {color, flash} }.
function romp.getVisibleGangZones(pid)
    local t = vis_of(pid)
    if not t then return {} end
    local out = {}
    for zid, v in pairs(t) do
        out[zid] = { color = v.color or 0xFFFFFFFF, flash = v.flash or 0 }
    end
    return out
end

-- ---------------- Hook event (dibungkus, tidak menimpa gamemode) ----------------

-- Join: kirim ulang semua zona yang sedang terlihat oleh pemain ini
-- (mis. zona yang di-show sebelum ia join tidak otomatis terlihat).
local _oldJoin = _G.onPlayerJoin
function _G.onPlayerJoin(pid, name)
    local t = vis_of(pid)
    if t then
        for zid, v in pairs(t) do
            local z = zones[zid]
            if z then
                send(pid, msg_set(zid, z, v.color or 0xFFFFFFFF,
                                 v.flash or 0))
            end
        end
    end
    if _oldJoin then return _oldJoin(pid, name) end
end

local _oldLeave = _G.onPlayerLeave
function _G.onPlayerLeave(pid, name, reason)
    visible[pid] = nil
    if _oldLeave then return _oldLeave(pid, name, reason) end
end
