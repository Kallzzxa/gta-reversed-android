-- ============================================================
-- RO-MP PVar/SVar ala SA-MP.
--
-- Dimuat OTOMATIS oleh server (lua_init) dari folder yang sama dengan
-- file gamemode, persis seperti dialog.lua & textdraw.lua & pickup.lua
-- & scoreboard.lua & vehicle.lua & nametag.lua & gangzone.lua.
--
-- PVar = variabel per-pemain (ala SA-MP PVars), SVar = variabel
-- global server (ala SA-MP SVars). Murni Lua helper, tidak perlu
-- paket protokol baru.
--
-- API (namespace romp):
--   PVar:
--     romp.setPVarInt(pid, name, value)
--     romp.setPVarFloat(pid, name, value)
--     romp.setPVarString(pid, name, value)
--     romp.getPVarInt(pid, name, default=0) -> int
--     romp.getPVarFloat(pid, name, default=0.0) -> float
--     romp.getPVarString(pid, name, default="") -> string
--     romp.deletePVar(pid, name) -> bool (true jika ada)
--     romp.getPVarType(pid, name) -> "int"|"float"|"string"|nil
--     romp.getPVarsUpperIndex(pid) -> int (jumlah var - 1, -1 jika kosong)
--     romp.getPVarNameAtIndex(pid, index) -> string|nil
--   SVar:
--     romp.setSVarInt(name, value)
--     romp.setSVarFloat(name, value)
--     romp.setSVarString(name, value)
--     romp.getSVarInt(name, default=0) -> int
--     romp.getSVarFloat(name, default=0.0) -> float
--     romp.getSVarString(name, default="") -> string
--     romp.deleteSVar(name) -> bool
--     romp.getSVarType(name) -> "int"|"float"|"string"|nil
--     romp.getSVarsUpperIndex() -> int
--     romp.getSVarNameAtIndex(index) -> string|nil
--
-- PVar otomatis dibersihkan saat pemain disconnect (via onPlayerLeave).
-- ============================================================

local pvars = {}  -- [pid] = { [name] = { type=..., value=... } }
local svars = {}  -- [name] = { type=..., value=... }

local function check_name(name)
    if type(name) ~= "string" or name == "" then
        error("varname harus string tidak kosong", 3)
    end
end

local function check_pid(pid)
    if type(pid) ~= "number" then
        error("playerid harus number", 3)
    end
end

-- ============ PVar ============

function romp.setPVarInt(pid, name, value)
    check_pid(pid); check_name(name)
    pvars[pid] = pvars[pid] or {}
    pvars[pid][name] = { type = "int", value = math.floor(tonumber(value) or 0) }
end

function romp.setPVarFloat(pid, name, value)
    check_pid(pid); check_name(name)
    pvars[pid] = pvars[pid] or {}
    pvars[pid][name] = { type = "float", value = tonumber(value) or 0.0 }
end

function romp.setPVarString(pid, name, value)
    check_pid(pid); check_name(name)
    pvars[pid] = pvars[pid] or {}
    pvars[pid][name] = { type = "string", value = tostring(value) }
end

local function getPVar(pid, name)
    local t = pvars[pid]
    if not t then return nil end
    return t[name]
end

function romp.getPVarInt(pid, name, default)
    local v = getPVar(pid, name)
    if not v then return default or 0 end
    if v.type == "int" then return v.value end
    if v.type == "float" then return math.floor(v.value) end
    return tonumber(v.value) or (default or 0)
end

function romp.getPVarFloat(pid, name, default)
    local v = getPVar(pid, name)
    if not v then return default or 0.0 end
    return tonumber(v.value) or (default or 0.0)
end

function romp.getPVarString(pid, name, default)
    local v = getPVar(pid, name)
    if not v then return default or "" end
    return tostring(v.value)
end

function romp.deletePVar(pid, name)
    check_pid(pid); check_name(name)
    local t = pvars[pid]
    if t and t[name] then
        t[name] = nil
        return true
    end
    return false
end

function romp.getPVarType(pid, name)
    local v = getPVar(pid, name)
    return v and v.type or nil
end

local function sorted_keys(t)
    local keys = {}
    for k in pairs(t) do keys[#keys + 1] = k end
    table.sort(keys)
    return keys
end

function romp.getPVarsUpperIndex(pid)
    local t = pvars[pid]
    if not t then return -1 end
    local n = 0
    for _ in pairs(t) do n = n + 1 end
    return n - 1
end

function romp.getPVarNameAtIndex(pid, index)
    local t = pvars[pid]
    if not t then return nil end
    return sorted_keys(t)[(index or 0) + 1]
end

-- ============ SVar ============

function romp.setSVarInt(name, value)
    check_name(name)
    svars[name] = { type = "int", value = math.floor(tonumber(value) or 0) }
end

function romp.setSVarFloat(name, value)
    check_name(name)
    svars[name] = { type = "float", value = tonumber(value) or 0.0 }
end

function romp.setSVarString(name, value)
    check_name(name)
    svars[name] = { type = "string", value = tostring(value) }
end

function romp.getSVarInt(name, default)
    local v = svars[name]
    if not v then return default or 0 end
    if v.type == "int" then return v.value end
    if v.type == "float" then return math.floor(v.value) end
    return tonumber(v.value) or (default or 0)
end

function romp.getSVarFloat(name, default)
    local v = svars[name]
    if not v then return default or 0.0 end
    return tonumber(v.value) or (default or 0.0)
end

function romp.getSVarString(name, default)
    local v = svars[name]
    if not v then return default or "" end
    return tostring(v.value)
end

function romp.deleteSVar(name)
    check_name(name)
    if svars[name] then
        svars[name] = nil
        return true
    end
    return false
end

function romp.getSVarType(name)
    local v = svars[name]
    return v and v.type or nil
end

function romp.getSVarsUpperIndex()
    local n = 0
    for _ in pairs(svars) do n = n + 1 end
    return n - 1
end

function romp.getSVarNameAtIndex(index)
    return sorted_keys(svars)[(index or 0) + 1]
end

-- Bersihkan PVar saat pemain disconnect (jangan bentrok dengan handler lain:
-- modul ini mendaftarkan via hook yang sudah ada bila tersedia)
if romp._pvar_orig_onLeave == nil then
    romp._pvar_orig_onLeave = romp.onPlayerLeave
    romp.onPlayerLeave = function(pid)
        pvars[pid] = nil
        if romp._pvar_orig_onLeave then
            return romp._pvar_orig_onLeave(pid)
        end
    end
end
