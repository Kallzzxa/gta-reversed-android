extends Node3D
## RO-MP PC — entry point.
## Membangun seluruh dunia dari kode: langit, kota, pemain, mobil, kamera, HUD.
## Fase multiplayer: menu koneksi -> RompNet (protokol TCP RO-MP) ->
## pemain remote (ped + nametag), chat ala SA-MP (T), kendaraan remote,
## scoreboard (TAB). Tanpa koneksi, game jalan sebagai sandbox offline.

const CityGenerator = preload("res://scripts/city_generator.gd")
const PlayerScript = preload("res://scripts/player.gd")
const CarScript = preload("res://scripts/car.gd")
const CameraRigScript = preload("res://scripts/camera_rig.gd")
const HudScript = preload("res://scripts/hud.gd")
const RompNet = preload("res://scripts/romp_net.gd")
const RemotePlayerScript = preload("res://scripts/remote_player.gd")
const RemoteCarScript = preload("res://scripts/remote_car.gd")
const MpMenuScript = preload("res://scripts/mp_menu.gd")
const MpChatScript = preload("res://scripts/mp_chat.gd")
const MpScoreboardScript = preload("res://scripts/mp_scoreboard.gd")

const POS_INTERVAL := 0.1  # kirim posisi 10Hz ala SA-MP

var player: CharacterBody3D
var cars: Array = []
var camera_rig: Node3D
var hud: CanvasLayer

var net: Node
var mp_menu: CanvasLayer
var mp_chat: CanvasLayer
var mp_scoreboard: CanvasLayer
var _pos_timer := 0.0
var _remotes: Dictionary = {}   # playerid -> RemotePlayer
var _remote_cars: Dictionary = {}  # vid -> RemoteCar
var _objects: Dictionary = {}   # objid -> MeshInstance3D (objek SA-MP)
var _mp_players: Array = []     # [{id, name}] untuk scoreboard
var _hostname := ""
var _ping_ms := -1
var _connecting := false


func _ready() -> void:
	_setup_environment()

	# Kota prosedural.
	var city = CityGenerator.new()
	add_child(city)
	city.generate()

	# Pemain.
	player = PlayerScript.new()
	player.position = Vector3(0, 1.0, 10.0)
	add_child(player)

	# Mobil: 1 dekat spawn + 2 parkir di jalan (properti lokal, client-side).
	var car_colors := [Color(0.75, 0.12, 0.1), Color(0.1, 0.25, 0.7), Color(0.9, 0.75, 0.15)]
	var car_pos := [Vector3(7, 1.0, 10.0), Vector3(-20, 1.0, -40.0), Vector3(30, 1.0, 60.0)]
	for i in range(3):
		var car = CarScript.new()
		car.body_color = car_colors[i]
		car.position = car_pos[i]
		car.rotation.y = randf() * TAU
		add_child(car)
		car.player = player
		cars.append(car)
	player.cars = cars

	# Kamera third-person.
	camera_rig = CameraRigScript.new()
	add_child(camera_rig)
	camera_rig.target = player
	player.rig = camera_rig

	# HUD.
	hud = HudScript.new()
	add_child(hud)
	hud.bind(player, cars)

	_setup_multiplayer()
	mp_menu.show_menu()


func _setup_environment() -> void:
	var env := WorldEnvironment.new()
	var e := Environment.new()
	e.background_mode = Environment.BG_SKY
	var sky := Sky.new()
	var sky_mat := ProceduralSkyMaterial.new()
	sky_mat.sky_top_color = Color(0.3, 0.55, 0.95)
	sky_mat.sky_horizon_color = Color(0.72, 0.83, 0.95)
	sky_mat.ground_bottom_color = Color(0.18, 0.18, 0.2)
	sky_mat.ground_horizon_color = Color(0.6, 0.65, 0.7)
	sky.sky_material = sky_mat
	e.sky = sky
	e.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
	e.ambient_light_energy = 0.6
	e.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	env.environment = e
	add_child(env)

	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-52, -35, 0)
	sun.shadow_enabled = true
	sun.light_energy = 1.15
	sun.directional_shadow_max_distance = 220.0
	add_child(sun)


func _setup_multiplayer() -> void:
	net = RompNet.new()
	add_child(net)
	net.connected.connect(_on_connected)
	net.connect_failed.connect(_on_connect_failed)
	net.disconnected.connect(_on_disconnected)
	net.player_list.connect(_on_player_list)
	net.player_joined.connect(_on_player_joined)
	net.player_left.connect(_on_player_left)
	net.player_pos.connect(_on_player_pos)
	net.chat_msg.connect(_on_chat_msg)
	net.vehicle_spawn.connect(_on_vehicle_spawn)
	net.vehicle_despawn.connect(_on_vehicle_despawn)
	net.vehicle_sync.connect(_on_vehicle_sync)
	net.vehicle_seat.connect(_on_vehicle_seat)
	net.teleport.connect(_on_teleport)
	net.ping_updated.connect(_on_ping_updated)
	net.label_spawn.connect(_on_label_spawn)
	net.label_despawn.connect(_on_label_despawn)
	net.label_text.connect(_on_label_text)
	net.label_pos.connect(_on_label_pos)
	net.object_spawn.connect(_on_object_spawn)
	net.object_despawn.connect(_on_object_despawn)
	net.object_move.connect(_on_object_move)
	net.object_update.connect(_on_object_update)

	mp_chat = MpChatScript.new()
	add_child(mp_chat)
	mp_chat.chat_sent.connect(func(t: String) -> void: net.send_chat(t))
	mp_chat.visible = false

	mp_scoreboard = MpScoreboardScript.new()
	add_child(mp_scoreboard)

	mp_menu = MpMenuScript.new()
	add_child(mp_menu)
	mp_menu.connect_requested.connect(_on_menu_connect)
	mp_menu.offline_requested.connect(_on_menu_offline)


func _on_menu_connect(nickname: String, host: String, port: int, password: String) -> void:
	_connecting = true
	_update_mp_status("… menghubungkan")
	net.connect_to(host, port, nickname, password)


func _on_menu_offline() -> void:
	mp_menu.hide_menu()
	_update_mp_status("○ Offline")


func _on_connected(pid: int, hostname: String, motd: String, maxplayers: int) -> void:
	_connecting = false
	_hostname = hostname
	_ping_ms = -1
	mp_menu.hide_menu()
	mp_chat.visible = true
	mp_chat.add_message(RompNet.CHAT_INFO, "Terhubung ke %s (id %d). T = chat, TAB = pemain." % [hostname, pid])
	if motd.strip_edges() != "" and motd != "-":
		mp_chat.add_message(RompNet.CHAT_INFO, "MOTD: " + motd)
	_update_mp_status("")


func _on_connect_failed(reason: String) -> void:
	_connecting = false
	mp_menu.show_menu()
	mp_menu.set_status("Gagal: " + reason, true)
	_update_mp_status("○ Offline")


func _on_disconnected() -> void:
	_clear_remotes()
	_mp_players.clear()
	_hostname = ""
	_ping_ms = -1
	mp_chat.visible = false
	if not _connecting:
		mp_menu.show_menu()
		mp_menu.set_status("Koneksi terputus.", true)
	_update_mp_status("○ Offline")


func _on_player_list(players: Array) -> void:
	_mp_players = players.duplicate()
	_refresh_scoreboard()


func _on_player_joined(pid: int, pname: String, is_npc: bool) -> void:
	if pid == net.player_id:
		return
	if not _remotes.has(pid):
		var rp = RemotePlayerScript.new()
		rp.player_name = pname
		rp.is_npc = is_npc
		add_child(rp)
		_remotes[pid] = rp
	_mp_players.append({"id": pid, "name": ("[BOT] " if is_npc else "") + pname})
	_refresh_scoreboard()


func _on_player_left(pid: int, _reason: int) -> void:
	if _remotes.has(pid):
		_remotes[pid].queue_free()
		_remotes.erase(pid)
	_mp_players = _mp_players.filter(func(p: Dictionary) -> bool: return int(p["id"]) != pid)
	_refresh_scoreboard()


func _on_player_pos(pid: int, x: float, y: float, z: float, heading: float,
		_state: int, _vmodel: int) -> void:
	if _remotes.has(pid):
		_remotes[pid].set_target(x, y, z, heading)


func _on_chat_msg(_sender: int, kind: int, text: String) -> void:
	mp_chat.add_message(kind, text)


func _on_vehicle_spawn(vid: int, model: int, x: float, y: float, z: float,
		angle: float, c1: int, c2: int, health: int, _driverid: int) -> void:
	if not _remote_cars.has(vid):
		var rc = RemoteCarScript.new()
		add_child(rc)
		_remote_cars[vid] = rc
	_remote_cars[vid].setup(vid, model, x, y, z, angle, c1, c2, health)


func _on_vehicle_despawn(vid: int) -> void:
	if _remote_cars.has(vid):
		_remote_cars[vid].queue_free()
		_remote_cars.erase(vid)


func _on_vehicle_sync(_driverid: int, vid: int, x: float, y: float, z: float,
		heading: float, health: int) -> void:
	if _remote_cars.has(vid):
		_remote_cars[vid].set_sync(x, y, z, heading, health)


func _on_vehicle_seat(_vid: int, _seat: int, _pid: int) -> void:
	pass  # v1: kursi tidak divisualkan; slot untuk iterasi berikut


func _on_teleport(x: float, y: float, z: float) -> void:
	# 0x27 POS_SET ala SA-MP SetPlayerPos.
	player.global_position = Vector3(x, y, z)
	player.velocity = Vector3.ZERO


## 3D text label ala SA-MP (Create3DTextLabel) — paket 0x28-0x2B.
var _labels: Dictionary = {}  # labelid -> Label3D


func _label_color(color: int) -> Color:
	# 0xRRGGBBAA ala SA-MP -> Godot Color.
	return Color(((color >> 24) & 0xFF) / 255.0,
			((color >> 16) & 0xFF) / 255.0,
			((color >> 8) & 0xFF) / 255.0,
			(color & 0xFF) / 255.0)


func _on_label_spawn(lid: int, atype: int, aid: int, x: float, y: float,
		z: float, drawdist: float, color: int, text: String) -> void:
	if _labels.has(lid):
		_labels[lid].queue_free()
	var lb := Label3D.new()
	lb.text = text
	lb.font_size = 64
	lb.pixel_size = 0.006
	lb.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	lb.no_depth_test = true
	lb.modulate = _label_color(color)
	lb.outline_size = 8
	lb.outline_modulate = Color(0, 0, 0, 1)
	if drawdist > 0:
		lb.visibility_range_end = drawdist
	if atype == 1 and _remotes.has(aid):
		# Attach ke pemain: ikut node remote (posisi di-sync client).
		_remotes[aid].add_child(lb)
		lb.position = Vector3(x, y, z)
	elif atype == 2 and _remote_cars.has(aid):
		# Attach ke kendaraan: ikut node mobil remote.
		_remote_cars[aid].add_child(lb)
		lb.position = Vector3(x, y, z)
	else:
		# Statis: posisi dunia.
		add_child(lb)
		lb.global_position = Vector3(x, y, z)
	_labels[lid] = lb


func _on_label_despawn(lid: int) -> void:
	if _labels.has(lid):
		_labels[lid].queue_free()
		_labels.erase(lid)


func _on_label_text(lid: int, color: int, text: String) -> void:
	if _labels.has(lid):
		_labels[lid].text = text
		_labels[lid].modulate = _label_color(color)


func _on_label_pos(lid: int, x: float, y: float, z: float) -> void:
	if _labels.has(lid):
		# Hanya label statis (tanpa parent entity) yang dipindah;
		# label attached mengikuti entity-nya.
		var lb: Label3D = _labels[lid]
		if lb.get_parent() == self:
			lb.global_position = Vector3(x, y, z)


# --- Objek ala SA-MP (CreateObject/MoveObject) ---
# Model asli GTA tak tersedia di client Godot, jadi objek digambar sebagai
# kubus semi-transparan + Label3D berisi model id di atasnya.

var _object_material: StandardMaterial3D  # dipakai ulang semua objek


func _get_object_material() -> StandardMaterial3D:
	if _object_material == null:
		_object_material = StandardMaterial3D.new()
		_object_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		_object_material.albedo_color = Color(0.2, 0.7, 1.0, 0.55)
		_object_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	return _object_material


func _on_object_spawn(oid: int, modelid: int, x: float, y: float, z: float,
		rx: float, ry: float, rz: float, drawdist: float, vw: int) -> void:
	if _objects.has(oid):
		_objects[oid].queue_free()
	var mi := MeshInstance3D.new()
	var box := BoxMesh.new()
	box.size = Vector3(1.0, 1.0, 1.0)
	mi.mesh = box
	mi.material_override = _get_object_material()
	var tag := Label3D.new()
	tag.text = "obj %d" % modelid
	tag.font_size = 48
	tag.pixel_size = 0.004
	tag.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	tag.no_depth_test = true
	tag.modulate = Color(1, 1, 1, 0.9)
	tag.outline_size = 6
	tag.outline_modulate = Color(0, 0, 0, 1)
	mi.add_child(tag)
	tag.position = Vector3(0, 0.9, 0)
	if drawdist > 0:
		mi.visibility_range_end = drawdist
	add_child(mi)
	mi.global_position = Vector3(x, y, z)
	mi.rotation_degrees = Vector3(rx, ry, rz)
	mi.set_meta("moving", false)
	_objects[oid] = mi


func _on_object_despawn(oid: int) -> void:
	if _objects.has(oid):
		_objects[oid].queue_free()
		_objects.erase(oid)


func _on_object_move(oid: int, tx: float, ty: float, tz: float, speed: float,
		trx: float, try_: float, trz: float) -> void:
	if not _objects.has(oid):
		return
	var mi: MeshInstance3D = _objects[oid]
	mi.set_meta("moving", true)
	mi.set_meta("target", Vector3(tx, ty, tz))
	mi.set_meta("speed", speed)
	var trot := Vector3(
		mi.rotation_degrees.x if trx <= -999.0 else trx,
		mi.rotation_degrees.y if try_ <= -999.0 else try_,
		mi.rotation_degrees.z if trz <= -999.0 else trz)
	mi.set_meta("target_rot", trot)


func _on_object_update(oid: int, x: float, y: float, z: float,
		rx: float, ry: float, rz: float) -> void:
	if not _objects.has(oid):
		return
	var mi: MeshInstance3D = _objects[oid]
	mi.set_meta("moving", false)
	mi.global_position = Vector3(x, y, z)
	mi.rotation_degrees = Vector3(rx, ry, rz)


func _process_objects(delta: float) -> void:
	for oid in _objects:
		var mi: MeshInstance3D = _objects[oid]
		if not mi.get_meta("moving"):
			continue
		var target: Vector3 = mi.get_meta("target")
		var speed: float = mi.get_meta("speed")
		var trot: Vector3 = mi.get_meta("target_rot")
		var pos := mi.global_position
		var dist := pos.distance_to(target)
		if dist < 0.001:
			mi.global_position = target
			mi.rotation_degrees = trot
			mi.set_meta("moving", false)
			continue
		var step: float = minf(speed * delta, dist)
		mi.global_position = pos + (target - pos).normalized() * step
		if dist > 0.0:
			var frac := step / dist
			mi.rotation_degrees = mi.rotation_degrees.lerp(trot, frac)
	mp_chat.add_message(RompNet.CHAT_INFO, "Di-teleport admin.")


func _on_ping_updated(ms: int) -> void:
	_ping_ms = ms
	_update_mp_status("")


func _process(delta: float) -> void:
	if net.is_connected:
		_pos_timer += delta
		if _pos_timer >= POS_INTERVAL:
			_pos_timer = 0.0
			_send_my_pos()
		_process_objects(delta)


func _send_my_pos() -> void:
	var p := player.global_position
	var heading := rad_to_deg(player.rotation.y)
	var state := RompNet.STATE_DRIVER if player.in_car else RompNet.STATE_ONFOOT
	# Mobil lokal bersifat client-side (tak ada vid server); kirim model
	# Infernus (411) sebagai penanda visual saja.
	var vmodel := 411 if player.in_car else 0
	net.send_pos(p.x, p.y, p.z, heading, state, vmodel)


func _clear_remotes() -> void:
	for pid in _remotes:
		_remotes[pid].queue_free()
	_remotes.clear()
	for vid in _remote_cars:
		_remote_cars[vid].queue_free()
	_remote_cars.clear()
	for lid in _labels:
		if is_instance_valid(_labels[lid]):
			_labels[lid].queue_free()
	_labels.clear()
	for oid in _objects:
		if is_instance_valid(_objects[oid]):
			_objects[oid].queue_free()
	_objects.clear()


func _refresh_scoreboard() -> void:
	var arr := _mp_players.duplicate()
	arr.append({"id": net.player_id, "name": "Kamu"})
	mp_scoreboard.set_data(_hostname, arr, net.player_id, _ping_ms)


func _update_mp_status(text: String) -> void:
	if text != "":
		hud.set_mp_status(text)
		return
	if net.is_connected:
		var ping_txt := "%dms" % _ping_ms if _ping_ms >= 0 else "-"
		hud.set_mp_status("● %s | %d pemain | %s" % [_hostname, _mp_players.size() + 1, ping_txt])
	else:
		hud.set_mp_status("○ Offline")


func _unhandled_input(event: InputEvent) -> void:
	# ESC di menu koneksi = main offline.
	if event is InputEventKey and event.pressed and not event.echo:
		if event.keycode == KEY_ESCAPE and mp_menu.visible:
			mp_menu.hide_menu()
			_update_mp_status("○ Offline")
			get_viewport().set_input_as_handled()
