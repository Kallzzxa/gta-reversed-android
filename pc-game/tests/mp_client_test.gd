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
	_net.label_spawn.connect(_on_lspawn)
	_net.actor_spawn.connect(_on_aspawn)
	_net.audio_play.connect(_on_aplay)
	_net.audio_stop.connect(_on_astop)
	_net.teleport.connect(_on_teleport)
	_net.ping_updated.connect(_on_ping)

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

	# --- 3b. 3D text label: peer kirim /lblgodot -> LABEL_SPAWN (0x28) ---
	_check(await _wait("label", 10.0), "label_spawn_diterima")

	# --- 3c. Actor statis: peer kirim /actorgodot -> ACTOR_SPAWN (0x32) ---
	_check(await _wait("actor", 10.0), "actor_spawn_diterima")

	# --- 3d. Audio stream: peer kirim /audiogodot -> AUDIO_PLAY (0x37) ---
	_check(await _wait("audio", 10.0), "audio_play_diterima")

	# --- 4. Teleport 0x27 via /tpgodot ---
	_net.send_chat("/tpgodot 500 600 70")
	_check(await _wait("teleport", 10.0), "pos_set_diterima")

	# --- 5. Ping client->server->client ---
	_check(await _wait("pong", 8.0), "ping_pong_terukur")

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


func _on_teleport(x: float, y: float, z: float) -> void:
	if absf(x - 500.0) < 0.01 and absf(y - 600.0) < 0.01 and absf(z - 70.0) < 0.01:
		_ev["teleport"] = true


func _on_ping(ms: int) -> void:
	if ms >= 0 and ms < 5000:
		_ev["pong"] = true
