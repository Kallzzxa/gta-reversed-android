-- json.lua — encoder/decoder JSON ringkas untuk gamemode RO-MP.
-- Dipakai untuk pesan NUI (Lua <-> WebView). Cukup untuk table, array,
-- string, number, boolean, nil. Bukan pengganti library JSON penuh.
-- Pakai: local json = dofile("json.lua")

local json = {}

local function isArray(t)
    if type(t) ~= "table" then return false end
    local n = 0
    for k, _ in pairs(t) do
        if type(k) ~= "number" then return false end
        n = n + 1
    end
    return n == #t
end

local function escapeStr(s)
    return s:gsub('[%z\1-\31\\"]', function(c)
        if c == '"' then return '\\"'
        elseif c == '\\' then return '\\\\'
        elseif c == '\n' then return '\\n'
        elseif c == '\r' then return '\\r'
        elseif c == '\t' then return '\\t'
        else return string.format('\\u%04x', c:byte()) end
    end)
end

function json.encode(v)
    local t = type(v)
    if t == "nil" then return "null"
    elseif t == "boolean" then return v and "true" or "false"
    elseif t == "number" then
        if v ~= v or v == math.huge or v == -math.huge then return "null" end
        return tostring(v)
    elseif t == "string" then return '"' .. escapeStr(v) .. '"'
    elseif t == "table" then
        local parts = {}
        if isArray(v) then
            for i = 1, #v do parts[#parts + 1] = json.encode(v[i]) end
            return "[" .. table.concat(parts, ",") .. "]"
        else
            for k, val in pairs(v) do
                parts[#parts + 1] = '"' .. escapeStr(tostring(k)) .. '":' .. json.encode(val)
            end
            return "{" .. table.concat(parts, ",") .. "}"
        end
    else
        return "null"
    end
end

-- Decoder: recursive descent sederhana.
function json.decode(s)
    local pos = 1
    local function err(msg) error("json.decode: " .. msg .. " di posisi " .. pos) end
    local function skipWs()
        while pos <= #s and s:sub(pos, pos):match("%s") do pos = pos + 1 end
    end
    local parseValue -- forward
    local function parseString()
        pos = pos + 1 -- lewati "
        local out = {}
        while pos <= #s do
            local c = s:sub(pos, pos)
            if c == '"' then pos = pos + 1; return table.concat(out) end
            if c == '\\' then
                local e = s:sub(pos + 1, pos + 1); pos = pos + 2
                if e == 'n' then out[#out + 1] = '\n'
                elseif e == 'r' then out[#out + 1] = '\r'
                elseif e == 't' then out[#out + 1] = '\t'
                elseif e == 'u' then
                    local hex = s:sub(pos, pos + 3); pos = pos + 4
                    out[#out + 1] = utf8.char(tonumber(hex, 16) or 63)
                else out[#out + 1] = e end
            else out[#out + 1] = c; pos = pos + 1 end
        end
        err("string tidak ditutup")
    end
    local function parseArray()
        pos = pos + 1
        local arr = {}
        skipWs()
        if s:sub(pos, pos) == "]" then pos = pos + 1; return arr end
        while true do
            arr[#arr + 1] = parseValue()
            skipWs()
            local c = s:sub(pos, pos); pos = pos + 1
            if c == "]" then return arr end
            if c ~= "," then err("harus ',' atau ']' di array") end
        end
    end
    local function parseObject()
        pos = pos + 1
        local obj = {}
        skipWs()
        if s:sub(pos, pos) == "}" then pos = pos + 1; return obj end
        while true do
            skipWs()
            if s:sub(pos, pos) ~= '"' then err("key object harus string") end
            local k = parseString()
            skipWs()
            if s:sub(pos, pos) ~= ":" then err("harus ':' setelah key") end
            pos = pos + 1
            obj[k] = parseValue()
            skipWs()
            local c = s:sub(pos, pos); pos = pos + 1
            if c == "}" then return obj end
            if c ~= "," then err("harus ',' atau '}' di object") end
        end
    end
    parseValue = function()
        skipWs()
        local c = s:sub(pos, pos)
        if c == '"' then return parseString()
        elseif c == "{" then return parseObject()
        elseif c == "[" then return parseArray()
        elseif s:sub(pos, pos + 3) == "true" then pos = pos + 4; return true
        elseif s:sub(pos, pos + 4) == "false" then pos = pos + 5; return false
        elseif s:sub(pos, pos + 3) == "null" then pos = pos + 4; return nil
        else
            local num = s:match("^%-?%d+%.?%d*[eE]?[+-]?%d*", pos)
            if num and #num > 0 then pos = pos + #num; return tonumber(num) end
            err("value tidak dikenal")
        end
    end
    local v = parseValue()
    skipWs()
    return v
end

return json
