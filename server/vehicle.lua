-- ============================================================
-- RO-MP Vehicle helper ala SA-MP — visualisasi NUI kendaraan.
--
-- Dimuat OTOMATIS oleh server (lua_init) dari folder yang sama dengan
-- file gamemode, persis seperti dialog.lua & textdraw.lua & pickup.lua.
-- State otoritatif kendaraan ada di C (romp_vehicle.c); file ini hanya
-- mengirim tampilan ke vehicle.html lewat 0x40 NUI_MESSAGE, di-diff tiap
-- detik (event onTick, dibungkus) memakai romp.getVehicles().
-- Jembatan: client game sesungguhnya merender kendaraan 3D dari paket
-- 0x13/0x14/0x15/0x16; halaman NUI hanya indikator 2D fallback.
--
-- API inti (di-register C di romp_lua.c, lihat PROTOCOL.md):
--   romp.createVehicle(model, x, y, z, angle, c1, c2, respawnDelayMs)
--   romp.destroyVehicle(vid), romp.setVehiclePos(vid, x, y, z [, angle]),
--   romp.getVehiclePos(vid), romp.setVehicleHealth(vid, hp [, killerid]),
--   romp.getVehicleHealth(vid), romp.damageVehicle(vid, amount [, killerid]),
--   romp.putPlayerInVehicle(pid, vid, seat), romp.removePlayerFromVehicle(pid),
--   romp.isPlayerInVehicle(pid), romp.getPlayerVehicle(pid),
--   romp.getPlayerVehicleSeat(pid), romp.getVehicleModel(vid),
--   romp.getVehicleColors(vid), romp.setVehicleColors(vid, c1, c2),
--   romp.getVehicleDriver(vid), romp.getVehiclePassenger(vid, seat),
--   romp.respawnVehicle(vid), romp.getVehicles()
-- Event: onPlayerEnterVehicle(pid, vid, seat),
--        onPlayerExitVehicle(pid, vid),
--        onVehicleDamage(vid, damagerid, newhealth),
--        onVehicleDeath(vid, killerid), onVehicleSpawn(vid)
--
-- Halaman UI: android/.../assets/romp/nui/vehicle.html
-- ============================================================

local src = debug.getinfo(1, "S").source:match("@?(.*/)")
local json = dofile((src or "./") .. "json.lua")

-- Snapshot terakhir per pemain: [pid] = { [vid] = "x|y|z|hp|seats" }
local snap = {}

local function send(pid, msg)
    return romp.sendNUIMessage(pid, json.encode(msg))
end

local function seatstr(v)
    -- seats[1] = sopir (indeks Lua 1-based dari getVehicles)
    local parts = {}
    for i = 1, 4 do
        local occ = v.seats and v.seats[i]
        parts[#parts + 1] = occ and tostring(occ) or "-"
    end
    return table.concat(parts, ",")
end

local function key_of(v)
    return string.format("%.1f|%.1f|%.1f|%d|%d|%s|%d|%d",
        v.x, v.y, v.z, v.health, v.destroyed and 1 or 0,
        seatstr(v), v.c1, v.c2)
end

local function show_full(pid, v)
    send(pid, { action = "showVehicle", id = "v" .. v.id,
        vid = v.id, model = v.model,
        x = v.x, y = v.y, z = v.z, angle = v.heading,
        c1 = v.c1, c2 = v.c2, health = v.health,
        destroyed = v.destroyed and true or false,
        driver = v.driver, seats = seatstr(v) })
end

local function tick_veh()
    for _, pl in ipairs(romp.getPlayers()) do
        local pid = pl.id
        local prev = snap[pid] or {}
        local seen = {}
        for _, v in ipairs(romp.getVehicles()) do
            seen[v.id] = true
            local k = key_of(v)
            if prev[v.id] == nil then
                show_full(pid, v)
            elseif prev[v.id] ~= k then
                send(pid, { action = "updateVehicle", id = "v" .. v.id,
                    vid = v.id, x = v.x, y = v.y, z = v.z,
                    health = v.health,
                    destroyed = v.destroyed and true or false,
                    driver = v.driver, seats = seatstr(v) })
            end
            prev[v.id] = k
        end
        for vid in pairs(prev) do
            if not seen[vid] then
                send(pid, { action = "hideVehicle", id = "v" .. vid })
                prev[vid] = nil
            end
        end
        snap[pid] = prev
    end
end

-- Bungkus handler gamemode (file ini dimuat SETELAH gamemode).
local _oldTick = _G.onTick
function _G.onTick()
    tick_veh()
    if _oldTick then _oldTick() end
end

local _oldLeave = _G.onPlayerLeave
function _G.onPlayerLeave(id, name, reason)
    snap[id] = nil
    if _oldLeave then return _oldLeave(id, name, reason) end
end
