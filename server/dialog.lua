-- ============================================================
-- RO-MP Dialog helper — router event onDialogResponse ala SA-MP.
--
-- Dimuat OTOMATIS oleh server (lua_init) dari folder yang sama dengan
-- file gamemode. Gamemode cukup definisikan:
--
--   function onDialogResponse(id, name, dialogId, response, listitem, inputtext)
--       -- id        = id pemain yang menjawab
--       -- name      = nickname pemain
--       -- dialogId  = id dialog dari romp.showDialog
--       -- response  = true  = tombol1 ditekan / item di-klik
--       --             false = tombol2 ditekan / dialog ditutup
--       -- listitem  = index item yang dipilih (0-based) untuk DIALOG_LIST,
--       --             -1 untuk style lain
--       -- inputtext = teks input (INPUT/PASSWORD) atau teks item
--       --             yang di-klik (LIST)
--   end
--
-- Client mengirim callback NUI "__dialog_response" dengan JSON:
--   {"dialogId":7,"response":true,"listitem":1,"inputtext":"B"}
-- ============================================================

-- Cari json.lua di folder yang sama dengan file ini (tidak tergantung CWD).
local src = debug.getinfo(1, "S").source:match("@?(.*/)")
local json = dofile((src or "./") .. "json.lua")

romp.onNUICallback("__dialog_response", function(pid, name, data)
    local ok, d = pcall(json.decode, data)
    if not ok or type(d) ~= "table" then
        romp.log("dialog_response: JSON tidak valid dari " .. tostring(name))
        return
    end
    local f = _G.onDialogResponse
    if type(f) ~= "function" then return end
    f(pid, name,
      tonumber(d.dialogId) or 0,
      d.response and true or false,
      tonumber(d.listitem) or -1,
      d.inputtext ~= nil and tostring(d.inputtext) or "")
end)
