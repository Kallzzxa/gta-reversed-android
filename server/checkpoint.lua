-- ============================================================
-- RO-MP Checkpoint helper ala SA-MP — masuk/keluar event via Lua + NUI.
--
-- Dimuat OTOMATIS oleh server (lua_init) dari folder yang sama dengan
-- file gamemode, persis seperti dialog.lua & textdraw.lua. Deteksi posisi
-- dilakukan server-side tiap detik (event onTick, dibungkus) memakai
-- romp.getPlayerPos(); tampilan marker dikirim via 0x40 NUI_MESSAGE ke
-- checkpoint.html.
--
-- API global (mirip SA-MP SetPlayerCheckpoint / SetPlayerRaceCheckpoint):
--   id = romp.createCheckpoint(ctype, x, y, z, size)
--        ctype: romp.CHECKPOINT_NORMAL (0) atau romp.CHECKPOINT_RACE (1)
--   romp.destroyCheckpoint(id) -> bool
--   romp.showCheckpointForPlayer(pid, id) -> bool
--        (satu checkpoint aktif per pemain ala SA-MP; show baru
--        menggantikan yang lama tanpa memicu event leave)
--   romp.hideCheckpointForPlayer(pid) -> bool
--   romp.getPlayerCheckpoint(pid) -> id | nil
--   romp.setCheckpointPos(id, x, y, z, size) -> bool
--        (opsional size: nil = tidak berubah)
--   romp.setRaceCheckpointNext(id, nx, ny, nz) -> bool
--        (titik target berikutnya untuk tipe race, dipakai UI/panah)
--   romp.getCheckpointPos(id) -> x, y, z, size | nil
--   romp.isPlayerInCheckpoint(pid) -> bool
--   romp.CHECKPOINT_NORMAL / romp.CHECKPOINT_RACE
--
-- Event (mirip SA-MP):
--   onPlayerEnterCheckpoint(pid, cpid)
--   onPlayerLeaveCheckpoint(pid, cpid)
--   onPlayerEnterRaceCheckpoint(pid, cpid)
--   onPlayerLeaveRaceCheckpoint(pid, cpid)
--
-- Halaman UI: android/.../assets/romp/nui/checkpoint.html
-- ============================================================

local src = debug.getinfo(1, "S").source:match("@?(.*/)")
local json = dofile((src or "./") .. "json.lua")

romp.CHECKPOINT_NORMAL = 0
romp.CHECKPOINT_RACE   = 1

local MAX_CP = 128

local cps = {}          -- [id] = {type, x,y,z, size, next={x,y,z} | nil}
local cpFree, nextCpid = {}, 1
local playerCp = {}     -- [pid] = cpid aktif
local playerIn = {}     -- [pid] = true jika di dalam radius

local function valid(cp)
    return type(cp) == "table" and type(cp.x) == "number"
        and type(cp.y) == "number" and type(cp.z) == "number"
        and type(cp.size) == "number" and cp.size > 0
end

local function num(v, d)
    return type(v) == "number" and v or d
end

local function msg_show(cpid, cp)
    return json.encode({
        action = "showCheckpoint", id = "c" .. cpid,
        type = cp.type, x = cp.x, y = cp.y, z = cp.z, size = cp.size,
        nextX = cp.next and cp.next.x or nil,
        nextY = cp.next and cp.next.y or nil,
        nextZ = cp.next and cp.next.z or nil,
    })
end

local function send(pid, payload)
    return romp.sendNUIMessage(pid, payload)
end

function romp.createCheckpoint(ctype, x, y, z, size)
    if ctype ~= 0 and ctype ~= 1 then return nil end
    if type(x) ~= "number" or type(y) ~= "number"
        or type(z) ~= "number" or type(size) ~= "number"
        or size <= 0 then return nil end
    local id = table.remove(cpFree) or nextCpid
    if id > MAX_CP then return nil end
    if id == nextCpid then nextCpid = nextCpid + 1 end
    cps[id] = { type = ctype, x = x, y = y, z = z, size = size }
    return id
end

function romp.destroyCheckpoint(id)
    local cp = cps[id]
    if not valid(cp) then return false end
    local payload = json.encode({ action = "hideCheckpoint", id = "c" .. id })
    for pid, cpid in pairs(playerCp) do
        if cpid == id then
            send(pid, payload)
            playerCp[pid] = nil
            playerIn[pid] = nil
        end
    end
    cps[id] = nil
    table.insert(cpFree, id)
    return true
end

function romp.showCheckpointForPlayer(pid, id)
    local cp = cps[id]
    if not valid(cp) then return false end
    -- Ganti aktif tanpa memicu event leave ala SA-MP.
    if not send(pid, msg_show(id, cp)) then return false end
    playerCp[pid] = id
    playerIn[pid] = nil
    return true
end

function romp.hideCheckpointForPlayer(pid)
    local cpid = playerCp[pid]
    if not cpid then return false end
    send(pid, json.encode({ action = "hideCheckpoint", id = "c" .. cpid }))
    playerCp[pid] = nil
    playerIn[pid] = nil
    return true
end

function romp.getPlayerCheckpoint(pid)
    return playerCp[pid]
end

function romp.setCheckpointPos(id, x, y, z, size)
    local cp = cps[id]
    if not valid(cp) then return false end
    if type(x) ~= "number" or type(y) ~= "number"
        or type(z) ~= "number" then return false end
    cp.x, cp.y, cp.z = x, y, z
    cp.size = num(size, cp.size)
    if cp.size <= 0 then cp.size = 1 end
    -- Refresh ke semua pemain yang sedang menampilkan checkpoint ini.
    local payload = msg_show(id, cp)
    for pid, cpid in pairs(playerCp) do
        if cpid == id then send(pid, payload) end
    end
    return true
end

function romp.setRaceCheckpointNext(id, nx, ny, nz)
    local cp = cps[id]
    if not valid(cp) then return false end
    if type(nx) ~= "number" or type(ny) ~= "number"
        or type(nz) ~= "number" then return false end
    cp.next = { x = nx, y = ny, z = nz }
    local payload = msg_show(id, cp)
    for pid, cpid in pairs(playerCp) do
        if cpid == id then send(pid, payload) end
    end
    return true
end

function romp.getCheckpointPos(id)
    local cp = cps[id]
    if not valid(cp) then return nil end
    return cp.x, cp.y, cp.z, cp.size
end

function romp.isPlayerInCheckpoint(pid)
    return playerIn[pid] == true
end

-- ---------------- Deteksi masuk/keluar (tiap detik via onTick) ----------------

local function fire(pid, cpid, ctype, entering)
    local cp = cps[cpid]
    if not cp then return end
    local ev
    if ctype == romp.CHECKPOINT_RACE then
        ev = entering and "onPlayerEnterRaceCheckpoint"
                      or "onPlayerLeaveRaceCheckpoint"
    else
        ev = entering and "onPlayerEnterCheckpoint"
                      or "onPlayerLeaveCheckpoint"
    end
    local fn = _G[ev]
    if type(fn) == "function" then
        local ok, err = pcall(fn, pid, cpid)
        if not ok then romp.log("error di " .. ev .. ": " .. tostring(err)) end
    end
end

local function tick_check()
    for pid, cpid in pairs(playerCp) do
        local cp = cps[cpid]
        if not valid(cp) then
            playerCp[pid] = nil
            playerIn[pid] = nil
        else
            local x, y, z = romp.getPlayerPos(pid)
            if x then
                local dx, dy, dz = x - cp.x, y - cp.y, z - cp.z
                local inside = (dx * dx + dy * dy + dz * dz)
                               <= cp.size * cp.size
                local was = playerIn[pid] == true
                if inside and not was then
                    playerIn[pid] = true
                    fire(pid, cpid, cp.type, true)
                elseif not inside and was then
                    playerIn[pid] = nil
                    fire(pid, cpid, cp.type, false)
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
    playerCp[id] = nil
    playerIn[id] = nil
    if _oldLeave then return _oldLeave(id, name) end
end
