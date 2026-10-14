-- ============================================================
-- RO-MP — Contoh Gamemode RP sederhana (Lua, ala FiveM).
--
-- File ini BUKAN gamemode aktif. Untuk memakainya, ubah baris
-- `gamemode` di server.cfg menjadi:
--     gamemode gamemode_contoh_rp.lua
-- lalu restart server.
--
-- Yang didemokan:
--   * Sistem uang per pemain (saldo tersimpan di file, tidak hilang
--     saat server restart)
--   * Command: /uang /kerja /kasih /dadu /topuang /bantuan
--   * Payday otomatis tiap 5 menit via onTick()
--   * Starter money untuk pemain baru
--
-- Semua logika di bawah hanya memakai API `romp.*` yang tersedia
-- (lihat gamemode.lua untuk daftar lengkap).
-- ============================================================

-- ---------- util ----------

local function rp(n)
    n = math.floor(n)
    local s = tostring(n)
    local out = ""
    while #s > 3 do
        out = "." .. s:sub(-3) .. out
        s = s:sub(1, -4)
    end
    return "Rp" .. s .. out
end

-- File data disimpan di folder yang sama dengan file script ini,
-- supaya tidak tergantung dari mana server dijalankan.
local function scriptDir()
    local src = debug.getinfo(1, "S").source -- "@path/ke/file.lua"
    if src:sub(1, 1) == "@" then src = src:sub(2) end
    return src:match("^(.*)/") or "."
end

local DATA_FILE = scriptDir() .. "/uang_data.lua"

local function simpan()
    local f, err = io.open(DATA_FILE, "w")
    if not f then
        romp.log("ContohRP: gagal simpan data: " .. tostring(err))
        return
    end
    f:write("-- data uang pemain, dibuat otomatis. jangan diedit manual.\nreturn {\n")
    for nama, saldo in pairs(Uang) do
        f:write(string.format("  [%q] = %d,\n", nama, math.floor(saldo)))
    end
    f:write("}\n")
    f:close()
end

local function muat()
    local ok, data = pcall(dofile, DATA_FILE)
    if ok and type(data) == "table" then
        return data
    end
    return {}
end

-- ---------- state ----------

Uang = muat()          -- nama pemain -> saldo (global supaya simpan() bisa akses)
local lastKerja = {}   -- id pemain -> tick ms terakhir /kerja
local detik = 0

local STARTER   = 50000   -- modal pemain baru
local PAYDAY    = 25000   -- gaji tiap 5 menit
local KERJA_MIN = 5000
local KERJA_MAX = 15000
local KERJA_CD  = 60000   -- cooldown /kerja (ms)

math.randomseed(os.time() + (romp.getTick() % 100000))

romp.log("ContohRP dimuat: sistem uang aktif (" .. rp(STARTER) .. " modal awal).")

local function saldo(id, nama)
    if Uang[nama] == nil then
        Uang[nama] = STARTER
        romp.sendChatToPlayer(id, romp.KIND_INFO,
            "*** Selamat datang! Kamu dapat modal awal " .. rp(STARTER) .. ".")
        simpan()
    end
    return Uang[nama]
end

-- ---------- events ----------

function onPlayerJoin(id, name)
    saldo(id, name) -- pastikan pemain baru dapat modal
    romp.sendChatToAll(romp.KIND_SYS,
        "*** " .. name .. " (" .. id .. ") datang ke kota. Ketik /bantuan.")
    return true
end

function onPlayerLeave(id, name, reason)
    simpan()
    return false -- biarkan pesan leave default tampil
end

function onPlayerCommand(id, name, cmd, args)
    local uang = saldo(id, name)

    if cmd == "bantuan" then
        romp.sendChatToPlayer(id, romp.KIND_INFO, "*** Perintah RP:")
        romp.sendChatToPlayer(id, romp.KIND_CHAT, "/uang - cek saldo")
        romp.sendChatToPlayer(id, romp.KIND_CHAT, "/kerja - cari uang (cooldown 60 dtk)")
        romp.sendChatToPlayer(id, romp.KIND_CHAT, "/kasih <id> <jumlah> - transfer uang")
        romp.sendChatToPlayer(id, romp.KIND_CHAT, "/dadu <taruhan> - judi 50:50, menang 2x lipat")
        romp.sendChatToPlayer(id, romp.KIND_CHAT, "/topuang - 5 orang terkaya")
        return true

    elseif cmd == "uang" then
        romp.sendChatToPlayer(id, romp.KIND_INFO,
            "*** Saldomu: " .. rp(uang))
        return true

    elseif cmd == "kerja" then
        local now = romp.getTick()
        local last = lastKerja[id] or 0
        if now - last < KERJA_CD then
            local sisa = math.ceil((KERJA_CD - (now - last)) / 1000)
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Istirahat dulu, kerja lagi dalam " .. sisa .. " detik.")
            return true
        end
        lastKerja[id] = now
        local dapat = math.random(KERJA_MIN, KERJA_MAX)
        Uang[name] = uang + dapat
        romp.sendChatToPlayer(id, romp.KIND_INFO,
            "*** Kamu bekerja dan dapat " .. rp(dapat)
            .. ". Saldo: " .. rp(Uang[name]) .. ".")
        simpan()
        return true

    elseif cmd == "kasih" then
        local target, jumlah = args:match("^(%d+)%s+(%d+)$")
        target, jumlah = tonumber(target), tonumber(jumlah)
        local namaTarget = target and romp.getPlayerName(target)
        if not target or not jumlah or not namaTarget then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Pakai: /kasih <id> <jumlah> (minimal Rp1.000)")
            return true
        end
        if target == id then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Tidak bisa kasih ke diri sendiri.")
            return true
        end
        if jumlah < 1000 then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Minimal transfer Rp1.000.")
            return true
        end
        if jumlah > uang then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Saldomu tidak cukup (" .. rp(uang) .. ").")
            return true
        end
        saldo(target, namaTarget) -- pastikan target terdaftar
        Uang[name] = uang - jumlah
        Uang[namaTarget] = Uang[namaTarget] + jumlah
        romp.sendChatToPlayer(id, romp.KIND_INFO,
            "*** Kamu memberi " .. rp(jumlah) .. " ke " .. namaTarget .. ".")
        romp.sendChatToPlayer(target, romp.KIND_INFO,
            "*** " .. name .. " memberimu " .. rp(jumlah) .. ".")
        simpan()
        return true

    elseif cmd == "dadu" then
        local taruhan = tonumber(args:match("^(%d+)$"))
        if not taruhan or taruhan < 1000 then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Pakai: /dadu <taruhan> (minimal Rp1.000)")
            return true
        end
        if taruhan > uang then
            romp.sendChatToPlayer(id, romp.KIND_INFO,
                "*** Saldomu tidak cukup (" .. rp(uang) .. ").")
            return true
        end
        local angka = math.random(1, 6)
        if angka >= 4 then -- menang: 4,5,6
            Uang[name] = uang + taruhan
            romp.sendChatToAll(romp.KIND_CHAT,
                "*** " .. name .. " lempar dadu: " .. angka
                .. " — MENANG " .. rp(taruhan) .. "!")
        else
            Uang[name] = uang - taruhan
            romp.sendChatToAll(romp.KIND_CHAT,
                "*** " .. name .. " lempar dadu: " .. angka
                .. " — kalah " .. rp(taruhan) .. ".")
        end
        simpan()
        return true

    elseif cmd == "topuang" then
        local daftar = {}
        for nama, s in pairs(Uang) do
            daftar[#daftar + 1] = { nama = nama, saldo = s }
        end
        table.sort(daftar, function(a, b) return a.saldo > b.saldo end)
        romp.sendChatToPlayer(id, romp.KIND_INFO, "*** 5 orang terkaya:")
        for i = 1, math.min(5, #daftar) do
            romp.sendChatToPlayer(id, romp.KIND_CHAT,
                i .. ". " .. daftar[i].nama .. " — " .. rp(daftar[i].saldo))
        end
        return true
    end

    return false -- /me /pm /players /help /q bawaan tetap jalan
end

function onTick()
    detik = detik + 1

    -- autosave tiap 60 detik
    if detik % 60 == 0 then
        simpan()
    end

    -- payday tiap 5 menit untuk semua yang online
    if detik % 300 == 0 then
        for _, p in ipairs(romp.getPlayers()) do
            if Uang[p.name] then
                Uang[p.name] = Uang[p.name] + PAYDAY
            end
        end
        simpan()
        romp.sendChatToAll(romp.KIND_INFO,
            "*** Payday! Semua pemain dapat " .. rp(PAYDAY) .. ".")
    end
end
