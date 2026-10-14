-- ============================================================
-- RO-MP Pickup helper ala SA-MP — event onPlayerPickup via Lua + NUI.
--
-- Dimuat OTOMATIS oleh server (lua_init) dari folder yang sama dengan
-- file gamemode, persis seperti dialog.lua & textdraw.lua & checkpoint.lua.
-- Deteksi posisi dilakukan server-side tiap detik (event onTick, dibungkus)
-- memakai romp.getPlayerPos(); tampilan marker dikirim via 0x40 NUI_MESSAGE
-- ke pickup.html (jembatan: client game sesungguhnya merender pickup 3D).
--
-- API global (mirip SA-MP AddStaticPickup / DestroyPickup):
--   id = romp.createPickup(model, ptype, x, y, z, respawnMs)
--        model:  id model GTA:SA ala SA-MP (mis. 1242 health, 1240 armour,
--               355 senjata AK47 — angka dipakai sebagai label UI saja)
--        ptype: romp.PICKUP_RESPAWN (muncul lagi setelah diambil),
--               romp.PICKUP_ONCE (hilang untuk pemain itu setelah diambil),
--               romp.PICKUP_ALWAYS (tetap tampil, event tiap masuk radius)
--        respawnMs: jeda respawn untuk PICKUP_RESPAWN (default 15000),
--               diabaikan untuk tipe lain
--   romp.destroyPickup(id) -> bool
--   romp.showPickupForPlayer(pid, id) -> bool
--   romp.hidePickupForPlayer(pid, id) -> bool
--   romp.isPickupVisibleForPlayer(pid, id) -> bool
--   romp.getPickupPos(id) -> x, y, z | nil
--   romp.setPickupPos(id, x, y, z) -> bool
--   romp.getPickupModel(id) -> model | nil
--   romp.setPickupRespawnTime(id, respawnMs) -> bool
--   romp.PICKUP_RESPAWN / romp.PICKUP_ONCE / romp.PICKUP_ALWAYS
--
-- Event (mirip SA-MP):
--   onPlayerPickup(pid, pickupid)
--
-- Halaman UI: android/.../assets/romp/nui/pickup.html
-- ============================================================

local src = debug.getinfo(1, "S").source:match("@?(.*/)")
local json = dofile((src or "./") .. "json.lua")

romp.PICKUP_RESPAWN = 0
romp.PICKUP_ONCE    = 1
romp.PICKUP_ALWAYS  = 2

local MAX_PICKUP = 256
local RADIUS = 1.5 -- radius ambil ala SA-MP (satuan dunia)

local ups = {}            -- [id] = {model, ptype, x,y,z, respawnMs}
local upFree, nextUpid = {}, 1
-- [pid] = { [upid] = { visible=bool, inside=bool, respawnAt=tick_ms | nil } }
local state = {}

local function valid(up)
    return type(up) == "table" and type(up.model) == "number"
        and type(up.x) == "number" and type(up.y) == "number"
        and type(up.z) == "number"
end

local function msg_show(upid, up)
    return json.encode({
        action = "showPickup", id = "p" .. upid,
        model = up.model, ptype = up.ptype,
        x = up.x, y = up.y, z = up.z,
    })
end

local function send(pid, payload)
    return romp.sendNUIMessage(pid, payload)
end

local function pstate(pid, upid)
    local ps = state[pid]
    if not ps then ps = {}; state[pid] = ps end
    local s = ps[upid]
    if not s then s = { visible = false, inside = false }; ps[upid] = s end
    return s
end

function romp.createPickup(model, ptype, x, y, z, respawnMs)
    if type(model) ~= "number" or model <= 0 then return nil end
    if ptype ~= 0 and ptype ~= 1 and ptype ~= 2 then return nil end
    if type(x) ~= "number" or type(y) ~= "number"
        or type(z) ~= "number" then return nil end
    if ptype == 0 then
        if type(respawnMs) ~= "number" or respawnMs < 1000 then
            respawnMs = 15000
        end
    else
        respawnMs = nil
    end
    local id = table.remove(upFree) or nextUpid
    if id > MAX_PICKUP then return nil end
    if id == nextUpid then nextUpid = nextUpid + 1 end
    ups[id] = { model = model, ptype = ptype, x = x, y = y, z = z,
                respawnMs = respawnMs }
    return id
end

function romp.destroyPickup(id)
    local up = ups[id]
    if not valid(up) then return false end
    local payload = json.encode({ action = "hidePickup", id = "p" .. id })
    for pid, ps in pairs(state) do
        if ps[id] and ps[id].visible then send(pid, payload) end
        ps[id] = nil
    end
    ups[id] = nil
    table.insert(upFree, id)
    return true
end

function romp.showPickupForPlayer(pid, id)
    local up = ups[id]
    if not valid(up) then return false end
    local s = pstate(pid, id)
    if s.visible then return true end
    if not send(pid, msg_show(id, up)) then return false end
    s.visible = true
    s.inside = false
    s.respawnAt = nil
    return true
end

function romp.hidePickupForPlayer(pid, id)
    local s = state[pid] and state[pid][id]
    if not s or not s.visible then return false end
    send(pid, json.encode({ action = "hidePickup", id = "p" .. id }))
    s.visible = false
    s.inside = false
    s.respawnAt = nil
    return true
end

function romp.isPickupVisibleForPlayer(pid, id)
    local s = state[pid] and state[pid][id]
    return s ~= nil and s.visible == true
end

function romp.getPickupPos(id)
    local up = ups[id]
    if not valid(up) then return nil end
    return up.x, up.y, up.z
end

function romp.setPickupPos(id, x, y, z)
    local up = ups[id]
    if not valid(up) then return false end
    if type(x) ~= "number" or type(y) ~= "number"
        or type(z) ~= "number" then return false end
    up.x, up.y, up.z = x, y, z
    local payload = msg_show(id, up)
    for pid, ps in pairs(state) do
        if ps[id] and ps[id].visible then send(pid, payload) end
    end
    return true
end

function romp.getPickupModel(id)
    local up = ups[id]
    if not valid(up) then return nil end
    return up.model
end

function romp.setPickupRespawnTime(id, respawnMs)
    local up = ups[id]
    if not valid(up) then return false end
    if type(respawnMs) ~= "number" or respawnMs < 1000 then return false end
    up.respawnMs = respawnMs
    return true
end

-- ---------------- Deteksi ambil pickup (tiap detik via onTick) ----------------

local function fire(pid, upid)
    local fn = _G.onPlayerPickup
    if type(fn) == "function" then
        local ok, err = pcall(fn, pid, upid)
        if not ok then romp.log("error di onPlayerPickup: " .. tostring(err)) end
    end
end

local function tick_check()
    local now = romp.getTick()
    for pid, ps in pairs(state) do
        local px, py, pz = romp.getPlayerPos(pid)
        for upid, s in pairs(ps) do
            local up = ups[upid]
            if not valid(up) then
                ps[upid] = nil
            else
                -- Respawn terlewati waktu untuk tipe RESPAWN.
                if s.respawnAt and now >= s.respawnAt then
                    send(pid, msg_show(upid, up))
                    s.visible = true
                    s.inside = false
                    s.respawnAt = nil
                end
                if s.visible and px then
                    local dx, dy, dz = px - up.x, py - up.y, pz - up.z
                    local inside = (dx * dx + dy * dy + dz * dz)
                                   <= RADIUS * RADIUS
                    if inside and not s.inside then
                        s.inside = true
                        fire(pid, upid)
                        if up.ptype == romp.PICKUP_ONCE then
                            send(pid, json.encode({ action = "hidePickup",
                                                    id = "p" .. upid }))
                            s.visible = false
                            s.inside = false
                        elseif up.ptype == romp.PICKUP_RESPAWN then
                            send(pid, json.encode({ action = "hidePickup",
                                                    id = "p" .. upid }))
                            s.visible = false
                            s.inside = false
                            s.respawnAt = now + (up.respawnMs or 15000)
                        end
                        -- PICKUP_ALWAYS: tetap tampil; event hanya saat
                        -- masuk ulang (inside sudah true, guard di atas).
                    elseif not inside and s.inside then
                        s.inside = false
                    end
                end
            end
        end
    end
end

-- Bungkus handler gamemode (file ini dimuat SETELAH gamemode).
local _oldTick = _G.onTick
function _G.onTick()
    tick_check()
    if _oldTick then _oldTick() end
end

local _oldLeave = _G.onPlayerLeave
function _G.onPlayerLeave(id, name)
    state[id] = nil
    if _oldLeave then return _oldLeave(id, name) end
end
