-- ============================================================
-- RO-MP player marker ala SA-MP — jembatan NUI untuk radar WebView.
--
-- State otoritatif (mode, warna, override per-viewer) hidup di server C
-- (romp_marker.c) dan diakses gamemode via API Lua:
--   romp.showPlayerMarkers(mode)            -- 0=off,1=global,2=streamed
--   romp.getPlayerMarkersMode()
--   romp.setPlayerColor(pid, color)         -- 0xRRGGBBAA
--   romp.getPlayerColor(pid)                -- ala SA-MP GetPlayerColor
--   romp.setPlayerMarkerForPlayer(viewer, showpid, color)
--   romp.clearPlayerMarkerForPlayer(viewer, showpid)
--   romp.getPlayerMarkerForPlayer(viewer, showpid)  -- warna efektif
--
-- File ini HANYA jembatan ke UI: tiap detik (onTick) mengirim posisi +
-- warna marker efektif tiap pemain yang terlihat ke radar mapicon.html
-- via pesan NUI 0x40 (romp.sendNUIMessage):
--   {"action":"playerMarkerMode","mode":2}
--   {"action":"playerMarker","pid":3,"x":100.0,"y":200.0,"z":3.0,
--    "color":4278190335}
--   {"action":"removePlayerMarker","pid":3}
--
-- Client game sesungguhnya merender marker dari paket 0x44/0x45
-- langsung; halaman WebView ini hanya fallback 2D (lihat mapicon.html).
-- ============================================================

local src = debug.getinfo(1, "S").source:match("@?(.*/)")
local json = dofile((src or "./") .. "json.lua")

-- [viewer_pid] = { mode = int|nil, known = { [showpid] = true } }
local viewers = {}

local function vs(pid, create)
    local v = viewers[pid]
    if not v and create then
        v = { mode = nil, known = {} }
        viewers[pid] = v
    end
    return v
end

local function send(pid, payload)
    return romp.sendNUIMessage(pid, payload)
end

local function push_tick()
    local players = romp.getPlayers() or {}
    local mode = romp.getPlayerMarkersMode()

    -- index cepat: id -> {interior, vw}
    local info = {}
    for _, p in ipairs(players) do
        info[p.id] = p
    end

    for _, vp in ipairs(players) do
        local pid = vp.id
        local v = vs(pid, true)

        if v.mode ~= mode then
            v.mode = mode
            send(pid, json.encode({ action = "playerMarkerMode",
                                    mode = mode }))
        end

        if mode == romp.MARKER_MODE_OFF then
            -- mode mati: bersihkan semua marker yang pernah dikirim
            for showpid in pairs(v.known) do
                send(pid, json.encode({ action = "removePlayerMarker",
                                        pid = showpid }))
            end
            v.known = {}
        else
            local seen = {}
            for _, tp in ipairs(players) do
                local tid = tp.id
                if tid ~= pid and tp.interior == vp.interior
                    and tp.vw == vp.vw then
                    seen[tid] = true
                    local x, y, z = romp.getPlayerPos(tid)
                    if x then
                        local color =
                            romp.getPlayerMarkerForPlayer(pid, tid) or
                            0xFFFFFFFF
                        send(pid, json.encode({
                            action = "playerMarker", pid = tid,
                            x = x, y = y, z = z, color = color,
                        }))
                    end
                end
            end
            -- hapus yang tak terlihat lagi (pindah vw / keluar)
            for showpid in pairs(v.known) do
                if not seen[showpid] then
                    send(pid, json.encode({ action = "removePlayerMarker",
                                            pid = showpid }))
                end
            end
            v.known = seen
        end
    end

    -- bersihkan state viewer yang sudah keluar
    for pid in pairs(viewers) do
        if not info[pid] then viewers[pid] = nil end
    end
end

-- Bungkus handler gamemode (file ini dimuat SETELAH gamemode).
local _oldTick = _G.onTick
function _G.onTick()
    push_tick()
    if _oldTick then _oldTick() end
end

local _oldLeave = _G.onPlayerLeave
function _G.onPlayerLeave(id, name)
    viewers[id] = nil
    if _oldLeave then return _oldLeave(id, name) end
end
