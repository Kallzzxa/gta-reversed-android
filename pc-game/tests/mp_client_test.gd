extends SceneTree
## Test integrasi headless klien multiplayer Godot (romp_net.gd).
## Dijalankan: godot --headless --path pc-game --script res://tests/mp_client_test.gd
## Env: ROMP_TEST_PORT (default 17799).
## Berbicara dengan server asli + 1 peer Python (nick "PeerPy").
## Mencetak "PASS <nama>" / "FAIL <nama>", exit code 0 bila semua lolos.

const RompNet = preload("res://scripts/romp_net.gd")

var _net = null
var _ev := {}
var _fails := 0
var _port := 17799


func _initialize() -> void:
	_run.call_deferred()


func _check(cond: bool, pname: String) -> void:
	if cond:
		print("PASS " + pname)
	else:
		_fails += 1
		print("FAIL " + pname)


func _wait(key: String, timeout_s: float) -> bool:
	var start := Time.get_ticks_msec()
	while float(Time.get_ticks_msec() - start) < timeout_s * 1000.0:
		if _ev.has(key):
			return true
		await process_frame
	return false


func _run() -> void:
	await process_frame
	var penv := OS.get_environment("ROMP_TEST_PORT")
	if penv != "":
		_port = int(penv)

	_net = RompNet.new()
	root.add_child(_net)
	_net.ping_interval = 1.0  # percepat test ping 0x30/0x31
	_net.connected.connect(_on_connected)
	_net.connect_failed.connect(_on_failed)
	_net.disconnected.connect(_on_disconnected)
	_net.player_list.connect(_on_list)
	_net.player_joined.connect(_on_joined)
	_net.player_pos.connect(_on_pos)
	_net.chat_msg.connect(_on_chat)
	_net.vehicle_spawn.connect(_on_vspawn)
	_net.vehicle_component.connect(_on_vcomp)
	_net.vehicle_paintjob.connect(_on_vpj)
	_net.vehicle_params.connect(_on_vparams)
	_net.vehicle_params_p.connect(_on_vparamsp)
	_net.label_spawn.connect(_on_lspawn)
	_net.actor_spawn.connect(_on_aspawn)
	_net.audio_play.connect(_on_aplay)
	_net.audio_stop.connect(_on_astop)
	_net.world_settings.connect(_on_world)
	_net.teleport.connect(_on_teleport)
	_net.ping_updated.connect(_on_ping)
	_net.marker_set.connect(_on_mset)
	_net.marker_mode.connect(_on_mmode)
	_net.death_msg.connect(_on_dmsg)
	_net.wanted_set.connect(_on_wanted)
	_net.team_set.connect(_on_team)
	_net.money_set.connect(_on_money)
	_net.veh_plate.connect(_on_plate)
	_net.spectate.connect(_on_spectate)
	_net.key_sync.connect(_on_key_sync)
	_net.anim_set.connect(_on_anim_set)
	_net.anim_clear.connect(_on_anim_clear)

	# --- 1. Handshake + WELCOME ---
	_net.connect_to("127.0.0.1", _port, "GodotTes", "rahasia")
	_check(await _wait("connected", 10.0), "welcome_diterima")
	_check(_net.player_id >= 0, "welcome_playerid_valid")

	# --- 2. Kirim chat; peer Python membalas "PEERHELLO" ---
	_net.send_chat("halo dari godot")
	_check(await _wait("peer_chat", 10.0), "chat_peer_diterima")
	_check(await _wait("peer_pos", 10.0), "ppos_peer_diterima")

	# --- 3. Kendaraan: peer kirim /veh -> VEH_SPAWN model 411 ---
	_check(await _wait("veh", 10.0), "veh_spawn_diterima")

	# --- 3a. Komponen kendaraan: /compgodot -> VEH_COMP (0x3A) + VEH_PAINTJOB (0x3C) ---
	_net.send_chat("/compgodot")
	_check(await _wait("comp", 10.0), "veh_comp_diterima")
	_check(await _wait("vpj", 10.0), "veh_paintjob_diterima")

	# --- 3a2. Vehicle params: /paramgodot -> VEH_PARAMS (0x3D) + VEH_PARAMS_P (0x3E) ---
	_net.send_chat("/paramgodot")
	_check(await _wait("vprm", 10.0), "veh_params_diterima")
	_check(await _wait("vprmp", 10.0), "veh_params_p_diterima")

	# --- 3b. 3D text label: peer kirim /lblgodot -> LABEL_SPAWN (0x28) ---
	_check(await _wait("label", 10.0), "label_spawn_diterima")

	# --- 3c. Actor statis: peer kirim /actorgodot -> ACTOR_SPAWN (0x32) ---
	_check(await _wait("actor", 10.0), "actor_spawn_diterima")

	# --- 3d. Audio stream: peer kirim /audiogodot -> AUDIO_PLAY (0x37) ---
	_check(await _wait("audio", 10.0), "audio_play_diterima")

	# --- 3e. World settings: peer kirim /worldgodot -> WORLD_SET (0x39) ---
	_check(await _wait("world", 10.0), "world_set_diterima")

	# --- 3e2. Key state ala SA-MP (0x4C KEY_STATE -> 0x4D KEY_SYNC):
	# peer kirim 0x4C keys=0x0025 (ACTION+FIRE+JUMP); server broadcast
	# 0x4D dengan pid peer ke viewer (Godot) ---
	_check(await _wait("keysync", 10.0), "key_sync_received")

	# --- 4. Teleport 0x27 via /tpgodot ---
	_net.send_chat("/tpgodot 500 600 70")
	_check(await _wait("teleport", 10.0), "pos_set_diterima")

	# --- 5. Ping client->server->client ---
	_check(await _wait("pong", 8.0), "ping_pong_terukur")

	# --- 5a. Player marker ala SA-MP (0x44/0x45): frame sintetik langsung
	# ke parser, verifikasi sinyal marker_set & marker_mode emit tepat ---
	_ev.erase("marker_set_ok")
	_ev.erase("marker_mode_ok")
	var mpay := StreamPeerBuffer.new()
	mpay.big_endian = false
	mpay.put_u8(5)          # pid
	mpay.put_u32(0xFF0000FF)  # warna merah opaque (LE)
	var mmode := StreamPeerBuffer.new()
	mmode.big_endian = false
	mmode.put_u8(2)         # mode streamed
	_net._inbuf.append_array(_mk_frame(RompNet.T_MARKER_SET, mpay.data_array))
	_net._inbuf.append_array(_mk_frame(RompNet.T_MARKER_MODE, mmode.data_array))
	_net._parse()
	_check(_ev.get("marker_set_ok", false), "marker_set_diterima")
	_check(_ev.get("marker_mode_ok", false), "marker_mode_diterima")

	# --- 5b. Kill feed ala SA-MP (0x47 DEATH_MSG): frame sintetik langsung
	# ke parser + end-to-end via /deathgodot (romp.sendDeathMessage) ---
	_ev.erase("death_ok")
	var dpay := StreamPeerBuffer.new()
	dpay.big_endian = false
	dpay.put_u8(3)    # killerid
	dpay.put_u8(7)    # victimid
	dpay.put_u8(24)   # reason (Deagle)
	_net._inbuf.append_array(_mk_frame(RompNet.T_DEATH_MSG, dpay.data_array))
	_net._parse()
	_check(_ev.get("death_ok", false), "death_msg_diterima")
	# end-to-end: server broadcast 0x47 sungguhan via sendDeathMessage
	_net.send_chat("/deathgodot")
	_check(await _wait("death_real", 10.0), "death_msg_server_diterima")

	# --- 5c. Wanted level ala SA-MP (0x48 WANTED_SET): frame sintetik
	# langsung ke parser + end-to-end via /wantedgodot
	# (romp.setPlayerWantedLevel -> broadcast 0x48) ---
	_ev.erase("wanted_ok")
	_ev.erase("wanted_real")
	var wpay := StreamPeerBuffer.new()
	wpay.big_endian = false
	wpay.put_u8(9)    # pid
	wpay.put_u8(4)    # level 4
	_net._inbuf.append_array(_mk_frame(RompNet.T_WANTED_SET, wpay.data_array))
	_net._parse()
	_check(_ev.get("wanted_ok", false), "wanted_set_diterima")
	# end-to-end: wanted 4 untuk GodotTes sendiri via gamemode
	_net.send_chat("/wantedgodot")
	_check(await _wait("wanted_real", 10.0), "wanted_set_server_diterima")

	# --- 5c2. Tim pemain ala SA-MP (0x53 TEAM_SET): frame sintetik
	# langsung ke parser + end-to-end via /teamgodot
	# (romp.setPlayerTeam -> broadcast 0x53) ---
	_ev.erase("team_ok")
	_ev.erase("team_real")
	var tpay := StreamPeerBuffer.new()
	tpay.big_endian = false
	tpay.put_u8(9)    # pid
	tpay.put_u8(2)    # team 2
	_net._inbuf.append_array(_mk_frame(RompNet.T_TEAM_SET, tpay.data_array))
	_net._parse()
	_check(_ev.get("team_ok", false), "team_set_diterima")
	# end-to-end: team 2 untuk GodotTes sendiri via gamemode
	await create_timer(2.5).timeout  # jeda anti-spam server (maks 3/2dtk)
	_net.send_chat("/teamgodot")
	_check(await _wait("team_real", 10.0), "team_set_server_diterima")

	# --- 5d. Uang pemain otoritatif ala SA-MP (0x49 MONEY_SET): frame
	# sintetik langsung ke parser + end-to-end via /moneygodot
	# (romp.givePlayerMoney -> 0x49 per-pemain) ---
	_ev.erase("money_ok")
	_ev.erase("money_real")
	var mnypay := StreamPeerBuffer.new()
	mnypay.big_endian = false
	mnypay.put_u8(9)     # pid
	mnypay.put_u32(12345)  # amount
	_net._inbuf.append_array(_mk_frame(RompNet.T_MONEY_SET, mnypay.data_array))
	_net._parse()
	_check(_ev.get("money_ok", false), "money_set_diterima")
	# end-to-end: givePlayerMoney(GodotTes, 7777) -> amount 7777 untuk diri
	await create_timer(2.5).timeout  # jeda anti-spam server (maks 3/2dtk)
	_net.send_chat("/moneygodot")
	_check(await _wait("money_real", 10.0), "money_set_server_diterima")

	# --- 5e. Plat nomor kendaraan ala SA-MP (0x4A VEH_PLATE): frame
	# sintetik langsung ke parser + end-to-end via /plategodot
	# (romp.setVehicleNumberPlate -> 0x4A broadcast vw-scoped) ---
	_ev.erase("plate_ok")
	_ev.erase("plate_real")
	var plpay := StreamPeerBuffer.new()
	plpay.big_endian = false
	plpay.put_u16(7)        # vid
	var pstr := "TES-PLAT".to_utf8_buffer()
	plpay.put_u8(pstr.size())  # len
	plpay.put_data(pstr)    # bytes
	_net._inbuf.append_array(_mk_frame(RompNet.T_VEH_PLATE, plpay.data_array))
	_net._parse()
	_check(_ev.get("plate_ok", false), "veh_plate_diterima")
	# end-to-end: /plategodot -> createVehicle + setNumberPlate "GODOT1"
	await create_timer(2.5).timeout  # jeda anti-spam server (maks 3/2dtk)
	_net.send_chat("/plategodot")
	_check(await _wait("plate_real", 10.0), "veh_plate_server_diterima")

	# --- 5f. Spectate ala SA-MP (0x4B SPECTATE): frame sintetik langsung
	# ke parser + end-to-end via /specgodot
	# (romp.playerSpectatePlayer(GodotTes, PeerPy, 1) -> 0x4B broadcast) ---
	_ev.erase("spec_ok")
	_ev.erase("spec_real")
	var spay := StreamPeerBuffer.new()
	spay.big_endian = false
	spay.put_u8(5)    # pid penonton
	spay.put_u8(1)    # spectating=1
	spay.put_u8(1)    # target_type=1 (pemain)
	spay.put_u16(9)   # target pid 9
	spay.put_u8(2)    # mode=2 (side)
	_net._inbuf.append_array(_mk_frame(RompNet.T_SPECTATE, spay.data_array))
	_net._parse()
	_check(_ev.get("spec_ok", false), "spectate_diterima")
	# end-to-end: /specgodot -> GodotTes spectate PeerPy mode fixed (1)
	await create_timer(2.5).timeout  # jeda anti-spam server (maks 3/2dtk)
	_net.send_chat("/specgodot")
	_check(await _wait("spec_real", 10.0), "spectate_server_diterima")

	# --- 5g. Animasi pemain ala SA-MP (0x4E ANIM_SET / 0x4F ANIM_CLEAR):
	# frame sintetik langsung ke parser + end-to-end via /animgodot
	# (romp.applyAnimationForPlayer(GodotTes, DANCING, dnce_m_a) -> 0x4E) ---
	_ev.erase("anim_ok")
	_ev.erase("anim_clear_ok")
	_ev.erase("anim_real")
	var apay := StreamPeerBuffer.new()
	apay.big_endian = false
	apay.put_u8(5)    # pid
	var alib := "DANCING".to_utf8_buffer()
	apay.put_u8(alib.size())  # liblen
	apay.put_data(alib)       # "DANCING"
	var aname := "dnce_m_a".to_utf8_buffer()
	apay.put_u8(aname.size())  # namelen
	apay.put_data(aname)       # "dnce_m_a"
	apay.put_u8(1)    # flags: loop
	apay.put_u16(500) # timems
	_net._inbuf.append_array(_mk_frame(RompNet.T_ANIM_SET, apay.data_array))
	_net._parse()
	_check(_ev.get("anim_ok", false), "anim_set_diterima")
	var acpay := StreamPeerBuffer.new()
	acpay.big_endian = false
	acpay.put_u8(5)   # pid
	_net._inbuf.append_array(_mk_frame(RompNet.T_ANIM_CLEAR, acpay.data_array))
	_net._parse()
	_check(_ev.get("anim_clear_ok", false), "anim_clear_diterima")
	# end-to-end: /animgodot -> applyAnimationForPlayer untuk GodotTes
	# (pid sendiri) + AEVT event -> 0x4E broadcast sungguhan
	await create_timer(2.5).timeout  # jeda anti-spam server (maks 3/2dtk)
	_net.send_chat("/animgodot")
	_check(await _wait("anim_real", 10.0), "anim_set_server_diterima")

	# --- 6. Password salah -> REJECT -> connect_failed ---
	_ev.erase("failed")
	_net.disconnect_now()
	await process_frame
	_net.connect_to("127.0.0.1", _port, "GodotTes", "salah")
	_check(await _wait("failed", 10.0), "reject_password_salah")

	if _fails == 0:
		print("SEMUA TEST GODOT MP LOLOS")
	else:
		print("GAGAL: %d cek" % _fails)
	quit(_fails)


func _on_connected(pid: int, hostname: String, _motd: String, _maxp: int) -> void:
	_ev["connected"] = true
	_ev["mypid"] = pid
	_check(hostname == "Godot MP Test", "welcome_hostname")


func _on_failed(reason: String) -> void:
	_ev["failed"] = reason


func _on_disconnected() -> void:
	pass


func _on_list(players: Array) -> void:
	for p in players:
		if str(p["name"]) == "PeerPy":
			_ev["peer_pid"] = int(p["id"])


func _on_joined(pid: int, pname: String, _is_npc: bool) -> void:
	if pname == "PeerPy":
		_ev["peer_pid"] = pid


func _on_pos(pid: int, x: float, y: float, z: float, _h: float, _s: int, _v: int) -> void:
	if _ev.get("peer_pid", -1) == pid and absf(x - 111.0) < 0.5 and absf(y - 222.0) < 0.5 and absf(z - 33.0) < 0.5:
		_ev["peer_pos"] = true


func _on_chat(_sender: int, _kind: int, text: String) -> void:
	if "PEERHELLO" in text:
		_ev["peer_chat"] = true


func _on_vspawn(_vid: int, model: int, _x: float, _y: float, _z: float,
		_angle: float, _c1: int, _c2: int, _hp: int, _d: int) -> void:
	if model == 411:
		_ev["veh"] = true


func _on_vcomp(_vid: int, compid: int, slot: int) -> void:
	if compid == 1000 and slot == 0:
		_ev["comp"] = true


func _on_vpj(_vid: int, paintjob: int) -> void:
	if paintjob == 2:
		_ev["vpj"] = true


func _on_vparams(_vid: int, engine: int, lights: int, _alarm: int,
		doors: int, _bonnet: int, _boot: int, objective: int) -> void:
	if engine == 1 and lights == 0 and doors == 1 and objective == 0:
		_ev["vprm"] = true


func _on_vparamsp(_vid: int, pid: int, objective: int,
		doorslocked: int) -> void:
	if pid == _net.player_id and objective == 1 and doorslocked == 0:
		_ev["vprmp"] = true


func _on_lspawn(_lid: int, atype: int, _aid: int, x: float, y: float,
		z: float, dd: float, color: int, text: String) -> void:
	if atype == 0 and text == "Label Godot" and absf(x - 30.0) < 0.5 \
			and absf(y - 40.0) < 0.5 and absf(z - 5.0) < 0.5 \
			and absf(dd - 100.0) < 0.5 and color == 0xFFFF00FF:
		_ev["label"] = true


func _on_aspawn(_aid: int, model: int, x: float, y: float,
		z: float, angle: float, health: int, invuln: bool, vw: int) -> void:
	if model == 7 and absf(x - 50.0) < 0.5 and absf(y - 60.0) < 0.5 \
			and absf(z - 3.0) < 0.5 and absf(angle - 90.0) < 0.5 \
			and health == 100 and not invuln and vw == 0:
		_ev["actor"] = true


func _on_aplay(url: String, usepos: bool, _x: float, _y: float, _z: float,
		_dist: float, _loop: bool) -> void:
	if url == "http://x.test/godot.mp3" and not usepos:
		_ev["audio"] = true


func _on_astop() -> void:
	_ev["audio_stop"] = true


func _on_world(gravity: float, weather: int, hour: int, minute: int) -> void:
	if absf(gravity - 0.003) < 0.0001 and weather == 17 and hour == 23 and minute == 45:
		_ev["world"] = true


func _on_teleport(x: float, y: float, z: float) -> void:
	if absf(x - 500.0) < 0.01 and absf(y - 600.0) < 0.01 and absf(z - 70.0) < 0.01:
		_ev["teleport"] = true


func _on_ping(ms: int) -> void:
	if ms >= 0 and ms < 5000:
		_ev["pong"] = true


## Frame sintetik ala protokol (u16 len LE + u8 tipe + payload) untuk
## menyuntik paket ke parser tanpa perlu server.
func _mk_frame(ftype: int, payload: PackedByteArray) -> PackedByteArray:
	var spb := StreamPeerBuffer.new()
	spb.big_endian = false
	spb.put_u16(payload.size() + 1)
	spb.put_u8(ftype)
	spb.put_data(payload)
	return spb.data_array


func _on_mset(pid: int, color: int) -> void:
	if pid == 5 and color == 0xFF0000FF:
		_ev["marker_set_ok"] = true


func _on_mmode(mode: int) -> void:
	if mode == 2:
		_ev["marker_mode_ok"] = true


func _on_dmsg(killerid: int, victimid: int, weaponid: int) -> void:
	# Frame sintetik: (3,7,24); end-to-end /deathgodot: killer 255,
	# victim = id sendiri, reason 51 (ledakan).
	if killerid == 3 and victimid == 7 and weaponid == 24:
		_ev["death_ok"] = true
	if killerid == 255 and weaponid == 51 and victimid == _net.player_id:
		_ev["death_real"] = true
		_ev["marker_mode_ok"] = true


func _on_wanted(pid: int, level: int) -> void:
	# Frame sintetik: (9,4); end-to-end /wantedgodot: level 4 untuk
	# pemain sendiri.
	if pid == 9 and level == 4:
		_ev["wanted_ok"] = true
	if pid == _net.player_id and level == 4:
		_ev["wanted_real"] = true


func _on_team(pid: int, team: int) -> void:
	# Frame sintetik: (9,2); end-to-end /teamgodot: team 2 untuk
	# pemain sendiri.
	if pid == 9 and team == 2:
		_ev["team_ok"] = true
	if pid == _net.player_id and team == 2:
		_ev["team_real"] = true


func _on_money(pid: int, amount: int) -> void:
	# Frame sintetik: pid=9, amount=12345; end-to-end /moneygodot:
	# givePlayerMoney(GodotTes, 7777) -> amount 7777 untuk pemain sendiri.
	if pid == 9 and amount == 12345:
		_ev["money_ok"] = true
	if pid == _net.player_id and amount == 7777:
		_ev["money_real"] = true


func _on_plate(vid: int, plate: String) -> void:
	# Frame sintetik: vid=7, plate="TES-PLAT"; end-to-end /plategodot:
	# setVehicleNumberPlate(vid, "GODOT1").
	if vid == 7 and plate == "TES-PLAT":
		_ev["plate_ok"] = true
	if plate == "GODOT1":
		_ev["plate_real"] = true


func _on_spectate(pid: int, spectating: int, target_type: int,
		target: int, mode: int) -> void:
	# Frame sintetik: (5,1,1,9,2); end-to-end /specgodot: GodotTes
	# (player_id) spectate PeerPy dengan mode fixed (1).
	if pid == 5 and spectating == 1 and target_type == 1 \
			and target == 9 and mode == 2:
		_ev["spec_ok"] = true
	if pid == _net.player_id and spectating == 1 \
			and target_type == 1 and mode == 1:
		_ev["spec_real"] = true


func _on_key_sync(pid: int, keys: int) -> void:
	# End-to-end: peer Python kirim 0x4C keys=0x0025 (ACTION+FIRE+JUMP);
	# server broadcast 0x4D dengan pid peer ke viewer (Godot).
	if keys == 0x0025 and pid == _ev.get("peer_pid", -2):
		_ev["keysync"] = true


func _on_anim_set(pid: int, animlib: String, animname: String,
		flags: int, timems: int) -> void:
	# Frame sintetik: (5, "DANCING", "dnce_m_a", loop, 500);
	# end-to-end /animgodot: pemain sendiri beranimasi DANCING/dnce_m_a.
	if pid == 5 and animlib == "DANCING" and animname == "dnce_m_a" \
			and flags == 1 and timems == 500:
		_ev["anim_ok"] = true
	if pid == _net.player_id and animlib == "DANCING" \
			and animname == "dnce_m_a":
		_ev["anim_real"] = true


func _on_anim_clear(pid: int) -> void:
	# Frame sintetik: pid 5.
	if pid == 5:
		_ev["anim_clear_ok"] = true
