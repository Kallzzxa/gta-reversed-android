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
const RemoteActorScript = preload("res://scripts/remote_actor.gd")
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
var _actors: Dictionary = {}    # actorid -> RemoteActor (actor SA-MP)
var _mp_players: Array = []     # [{id, name}] untuk scoreboard
var _my_nick: String = ""      # nickname pemain lokal (untuk kill feed)
var _hostname := ""
var _ping_ms := -1
var _connecting := false
var _sky_mat: ProceduralSkyMaterial  # untuk terapkan cuaca dari server
var _sun: DirectionalLight3D  # untuk terapkan jam dunia dari server
var _world_gravity := 0.008  # gravity ala SA-MP (default GTA:SA)
var _default_gravity := 9.8


func _ready() -> void:
	_setup_environment()

	# Kota prosedural.
	var city = CityGenerator.new()
	add_child(city)
	city.generate()

	# Pemain.
	player = PlayerScript.new()
	player.position = Vector3(0, 1.0, 10.0)
	_default_gravity = player.gravity
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
	_sky_mat = sky_mat
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
	_sun = sun
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
	net.vehicle_component.connect(_on_vehicle_component)
	net.vehicle_component_remove.connect(_on_vehicle_component_remove)
	net.vehicle_paintjob.connect(_on_vehicle_paintjob)
	net.vehicle_params.connect(_on_vehicle_params)
	net.vehicle_params_p.connect(_on_vehicle_params_p)
	net.veh_plate.connect(_on_vehicle_plate)
	net.spectate.connect(_on_spectate)
	net.key_sync.connect(_on_key_sync)
	net.anim_set.connect(_on_anim_set)
	net.anim_clear.connect(_on_anim_clear)
	net.attach_obj_set.connect(_on_attach_obj_set)
	net.attach_obj_remove.connect(_on_attach_obj_remove)
	net.skill_set.connect(_on_skill_set)
	net.marker_set.connect(_on_marker_set)
	net.marker_mode.connect(_on_marker_mode)
	net.death_msg.connect(_on_death_msg)
	net.wanted_set.connect(_on_wanted_set)
	net.team_set.connect(_on_team_set)
	net.clock_set.connect(_on_clock_set)
	net.money_set.connect(_on_money_set)
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
	net.actor_spawn.connect(_on_actor_spawn)
	net.actor_despawn.connect(_on_actor_despawn)
	net.actor_pos.connect(_on_actor_pos)
	net.actor_anim.connect(_on_actor_anim)
	net.actor_health.connect(_on_actor_health)
	net.audio_play.connect(_on_audio_play)
	net.audio_stop.connect(_on_audio_stop)
	net.world_settings.connect(_on_world_settings)

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
	_my_nick = nickname
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
	_anim_state.erase(pid)
	_keys_state.erase(pid)
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


func _on_vehicle_component(vid: int, compid: int, slot: int) -> void:
	# 0x3A VEH_COMP ala SA-MP AddVehicleComponent.
	if _remote_cars.has(vid):
		_remote_cars[vid].set_component(slot, compid)


func _on_vehicle_component_remove(vid: int, compid: int) -> void:
	# 0x3B VEH_COMP_REMOVE ala SA-MP RemoveVehicleComponent.
	if _remote_cars.has(vid):
		_remote_cars[vid].remove_component(compid)


func _on_vehicle_paintjob(vid: int, paintjob: int) -> void:
	# 0x3C VEH_PAINTJOB ala SA-MP ChangeVehiclePaintjob.
	if _remote_cars.has(vid):
		_remote_cars[vid].set_paintjob(paintjob)


func _on_vehicle_plate(vid: int, plate: String) -> void:
	# 0x4A VEH_PLATE ala SA-MP SetVehicleNumberPlate.
	if _remote_cars.has(vid):
		_remote_cars[vid].set_number_plate(plate)


## Spectate ala SA-MP (0x4B SPECTATE): status penonton per pemain
## (TogglePlayerSpectating / PlayerSpectatePlayer / PlayerSpectateVehicle).
var _spectate_state := {}  # pid -> {target_type, target, mode}


func _on_spectate(pid: int, spectating: int, target_type: int,
		target: int, mode: int) -> void:
	# 0x4B SPECTATE: broadcast vw-scoped ke vw penonton + resync.
	if spectating == 1:
		_spectate_state[pid] = {"target_type": target_type,
			"target": target, "mode": mode}
	else:
		_spectate_state.erase(pid)
	if pid == net.player_id:
		# Pemain lokal (berhenti) spectate: alihkan kamera mengikuti
		# target, sembunyikan ped lokal ala SA-MP.
		_apply_local_spectate(spectating == 1, target_type, target)
	elif _remotes.has(pid):
		# Pemain remote (berhenti) spectate: sembunyikan / tampilkan
		# ped-nya (tetap ada di daftar pemain & scoreboard).
		_remotes[pid].visible = spectating == 0


func _apply_local_spectate(on: bool, target_type: int, target: int) -> void:
	if on:
		var node: Node3D = null
		if target_type == 1 and _remotes.has(target):
			node = _remotes[target]
		elif target_type == 2 and _remote_cars.has(target):
			node = _remote_cars[target]
		if node:
			camera_rig.target = node
			player.visible = false
			mp_chat.add_message(RompNet.CHAT_INFO,
				"*** 👁 Mode spectate: kamera mengikuti target.")
		else:
			mp_chat.add_message(RompNet.CHAT_INFO,
				"*** 👁 Mode spectate aktif (target belum ter-stream).")
	else:
		camera_rig.target = player
		player.visible = true
		mp_chat.add_message(RompNet.CHAT_INFO,
			"*** 👁 Mode spectate berhenti.")


## Key state ala SA-MP (0x4D KEY_SYNC): bitmask tombol per pemain
## (GetPlayerKeys / OnPlayerKeyStateChange ala SA-MP).
var _keys_state := {}  # pid -> keys (u16 bitmask)


func _on_key_sync(pid: int, keys: int) -> void:
	# 0x4D KEY_SYNC: simpan state per pemain; cukup simpan (anti spam —
	# tidak ada chat/efek otomatis tiap berubah). Handler game membaca
	# _keys_state[pid] saat butuh (mis. pose nembak/jongkok ped remote).
	_keys_state[pid] = keys


## Animasi pemain ala SA-MP (0x4E ANIM_SET / 0x4F ANIM_CLEAR —
## romp.applyAnimationForPlayer / clearAnimationsForPlayer).
var _anim_state := {}  # pid -> { animlib, animname, flags, timems }


func _on_anim_set(pid: int, animlib: String, animname: String,
		flags: int, timems: int) -> void:
	# Simpan state animasi per pemain; Godot tak punya anim lib GTA jadi
	# cukup tampilkan di nametag ped remote (mis. "Nama [💃 dnce_m_a]").
	# TODO(anim): putar animasi GTA nyata bila anim lib tersedia.
	_anim_state[pid] = {"animlib": animlib, "animname": animname,
		"flags": flags, "timems": timems}
	if _remotes.has(pid):
		_remotes[pid].set_animation_tag(animname)
	if pid == net.player_id:
		mp_chat.add_message(RompNet.CHAT_INFO,
			"*** Kamu beranimasi: %s %s." % [animlib, animname])


func _on_anim_clear(pid: int) -> void:
	_anim_state.erase(pid)
	if _remotes.has(pid):
		_remotes[pid].set_animation_tag("")
	if pid == net.player_id:
		mp_chat.add_message(RompNet.CHAT_INFO,
			"*** Animasi dihentikan.")


func _on_vehicle_params(vid: int, engine: int, lights: int, alarm: int,
		doors: int, bonnet: int, boot: int, objective: int) -> void:
	# 0x3D VEH_PARAMS ala SA-MP SetVehicleParamsEx.
	if _remote_cars.has(vid):
		_remote_cars[vid].set_params(engine, lights, alarm, doors,
			bonnet, boot, objective)


func _on_vehicle_params_p(vid: int, pid: int, objective: int,
		doorslocked: int) -> void:
	# 0x3E VEH_PARAMS_P ala SA-MP SetVehicleParamsForPlayer.
	if _remote_cars.has(vid):
		_remote_cars[vid].set_params_for_player(pid, objective, doorslocked)


## Attached object ala SA-MP (0x3F set/remove) — placeholder kubus di
## remote player (lihat remote_player.gd).
func _on_attach_obj_set(pid: int, index: int, modelid: int, bone: int,
		off: Vector3, rot: Vector3, scale: Vector3) -> void:
	if _remotes.has(pid):
		_remotes[pid].set_attach(index, modelid, bone, off, rot, scale)


func _on_attach_obj_remove(pid: int, index: int) -> void:
	if _remotes.has(pid):
		_remotes[pid].remove_attach(index)


## Weapon skill ala SA-MP (0x43): level skill senjata pemain. Disimpan
## per-pemain (hanya dikirim ke pemiliknya); dipakai handling senjata
## client (akurasi/kecepatan bidik ala GTA:SA).
var _player_skills := {}  # pid -> Array 11 level

func _on_skill_set(pid: int, skill: int, level: int) -> void:
	if not _player_skills.has(pid):
		_player_skills[pid] = [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1]
	if skill >= 0 and skill < 11:
		_player_skills[pid][skill] = level


## Wanted level ala SA-MP (0x48): level wanted per-pemain (0=bersih ..
## 6=paling dicari). Disimpan per-pemain; dipakai HUD (bintang wanted)
## dan logika polisi client (cop car saat level >= 3).
var _player_wanted := {}  # pid -> int level

func _on_wanted_set(pid: int, level: int) -> void:
	_player_wanted[pid] = level
	if level >= 3:
		mp_chat.add_message(RompNet.CHAT_INFO,
			"*** 🚔 %s kini wanted level %d!" % [_mp_name_of(pid), level])


## Tim pemain ala SA-MP (0x53 TEAM_SET): tim per-pemain (0..254 = id tim,
## 255 = TEAM_NONE tanpa tim ala SA-MP NO_TEAM). Disimpan per-pemain;
## dipakai warna nametag/scoreboard per tim (gangwar/dm).
var _player_team := {}  # pid -> int team

func _on_team_set(pid: int, team: int) -> void:
	_player_team[pid] = team
	if team == 255:
		mp_chat.add_message(RompNet.CHAT_INFO,
			"*** %s keluar dari tim (tanpa tim)." % _mp_name_of(pid))
	else:
		mp_chat.add_message(RompNet.CHAT_INFO,
			"*** %s kini di tim %d." % [_mp_name_of(pid), team])


## Toggle jam pemain ala SA-MP (0x54 CLOCK_SET, dari romp.togglePlayerClock):
## jam berjalan (enabled) atau dibekukan. Per-pemain — client hanya menerima
## miliknya sendiri (+ resync saat join). Disimpan; client tak boleh
## memajukan jam dunia sendiri bila server membekukannya.
var _local_clock_enabled := true

func _on_clock_set(pid: int, enabled: bool) -> void:
	if pid == net.player_id:
		_local_clock_enabled = enabled
		mp_chat.add_message(RompNet.CHAT_INFO,
			"*** Jam duniamu %s oleh server." %
				("berjalan kembali" if enabled else "dibekukan"))


## Uang pemain otoritatif ala SA-MP (0x49 MONEY_SET): saldo pemain pid
## berubah menjadi amount. Per-pemain — client hanya menerima saldo milik
## sendiri. Disimpan untuk HUD uang (TODO: tampilkan di hud.gd).
var _player_money := {}  # pid -> int amount

## Format rupiah ala gamemode RP ("Rp1.000.000").
func _rp(n: int) -> String:
	var neg := n < 0
	var s := str(absi(n))
	var out := ""
	while s.length() > 3:
		out = "." + s.right(3) + out
		s = s.left(s.length() - 3)
	out = s + out
	return ("-Rp" if neg else "Rp") + out

func _on_money_set(pid: int, amount: int) -> void:
	_player_money[pid] = amount
	if pid == net.player_id:
		mp_chat.add_message(RompNet.CHAT_INFO,
			"*** 💰 Saldomu kini: %s" % [_rp(amount)])


## Player marker ala SA-MP (paket 0x44/0x45): warna marker efektif
## per-pemain (0xRRGGBBAA) dan mode tampilan (0=off, 1=global,
## 2=streamed). Marker divisualkan lewat warna nametag 3D pemain
## remote (lihat remote_player.gd).
var marker_colors: Dictionary = {}  # pid -> warna 0xRRGGBBAA
var marker_mode: int = 2


func _on_marker_set(pid: int, color: int) -> void:
	marker_colors[pid] = color
	if _remotes.has(pid):
		_remotes[pid].set_marker_color(color)


func _on_marker_mode(mode: int) -> void:
	# Mode 0 (off) cukup disimpan: nametag tetap dipakai fitur nametag,
	# warna marker di radar Android ditangani mapicon.html.
	marker_mode = mode


## Kill feed ala SA-MP (SendDeathMessage / OnPlayerDeath) — paket 0x47.
const DEATH_REASON_NAMES := {
	49: "Tabrakan", 50: "Baling-baling", 51: "Ledakan",
	53: "Tenggelam", 54: "Jatuh",
}

func _mp_name_of(pid: int) -> String:
	if pid == net.player_id:
		return _my_nick if _my_nick != "" else "Saya"
	for p: Dictionary in _mp_players:
		if int(p["id"]) == pid:
			return String(p["name"])
	return "Pemain#%d" % pid

func _death_reason_name(weaponid: int) -> String:
	if DEATH_REASON_NAMES.has(weaponid):
		return String(DEATH_REASON_NAMES[weaponid])
	if weaponid >= 1 and weaponid <= 46:
		return "Senjata#%d" % weaponid
	return "???"


func _on_death_msg(killerid: int, victimid: int, weaponid: int) -> void:
	# Baris kill feed ala SA-MP: "pembunuh [senjata] korban"
	# (killerid 255 = tak ada pembunuh, mis. jatuh/ledakan sendiri).
	var victim_name := _mp_name_of(victimid)
	var line: String
	if killerid == 255:
		line = "%s ☠ [%s]" % [victim_name, _death_reason_name(weaponid)]
	else:
		line = "%s ☠ [%s] %s" % [_mp_name_of(killerid),
				_death_reason_name(weaponid), victim_name]
	mp_chat.add_message(RompNet.CHAT_INFO, line)


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


## Actor statis ala SA-MP (CreateActor) — paket 0x32-0x36.
func _on_actor_spawn(actorid: int, modelid: int, x: float, y: float, z: float,
		angle: float, health: int, invulnerable: bool, _vw: int) -> void:
	if _actors.has(actorid):
		_actors[actorid].queue_free()
	var ra = RemoteActorScript.new()
	add_child(ra)
	ra.setup(actorid, modelid, x, y, z, angle, health, invulnerable)
	_actors[actorid] = ra


func _on_actor_despawn(actorid: int) -> void:
	if _actors.has(actorid):
		_actors[actorid].queue_free()
		_actors.erase(actorid)


func _on_actor_pos(actorid: int, x: float, y: float, z: float,
		angle: float) -> void:
	if _actors.has(actorid):
		_actors[actorid].set_pos(x, y, z, angle)


func _on_actor_anim(actorid: int, anim: String) -> void:
	if _actors.has(actorid):
		_actors[actorid].set_anim(anim)


func _on_actor_health(actorid: int, health: int) -> void:
	if _actors.has(actorid):
		_actors[actorid].set_health(health)


# --- Audio stream ala SA-MP (PlayAudioStreamForPlayer) ---
# URL diunduh via HTTPRequest lalu diputar sebagai AudioStream
# (format terdeteksi dari magic byte: mp3/ogg/wav).

var _audio_player: AudioStreamPlayer  # dibuat saat dibutuhkan


func _audio_ensure_player() -> AudioStreamPlayer:
	if _audio_player == null:
		_audio_player = AudioStreamPlayer.new()
		_audio_player.bus = "Master"
		add_child(_audio_player)
	return _audio_player


func _on_audio_play(url: String, usepos: bool, x: float, y: float, z: float,
		distance: float, loop: bool) -> void:
	if url == "":
		return
	if usepos:
		# TODO: atenuasi volume dari jarak ke posisi sumber (x,y,z,distance).
		print("[audio] posisional (%.1f, %.1f, %.1f dist %.0f): %s"
				% [x, y, z, distance, url])
	var pl := _audio_ensure_player()
	pl.stop()
	var req := HTTPRequest.new()
	add_child(req)
	var err := req.request(url)
	if err != OK:
		push_warning("[audio] gagal request: " + url)
		req.queue_free()
		return
	var res: Array = await req.request_completed
	req.queue_free()
	if res[0] != HTTPRequest.RESULT_SUCCESS or res[1] != 200:
		push_warning("[audio] unduh gagal (%s): %s" % [str(res[1]), url])
		return
	var data: PackedByteArray = res[3]
	var stream: AudioStream = _audio_make_stream(url, data)
	if stream == null:
		push_warning("[audio] format tak dikenali: " + url)
		return
	if "loop" in stream:
		stream.set("loop", loop)
	pl.stream = stream
	pl.play()


func _audio_make_stream(url: String, data: PackedByteArray) -> AudioStream:
	# Deteksi format dari magic byte dulu, fallback ke ekstensi URL.
	var ext := url.get_extension().to_lower()
	if data.size() >= 4 and data.slice(0, 4) == PackedByteArray([0x4F, 0x67, 0x67, 0x53]):  # "OggS"
		ext = "ogg"
	elif data.size() >= 3 and data.slice(0, 3) == PackedByteArray([0x49, 0x44, 0x33]):  # "ID3"
		ext = "mp3"
	elif data.size() >= 2 and data[0] == 0xFF and (data[1] & 0xE0) == 0xE0:  # frame sync mp3
		ext = "mp3"
	elif data.size() >= 12 and data.slice(0, 4) == PackedByteArray([0x52, 0x49, 0x46, 0x46]):  # "RIFF"
		ext = "wav"
	match ext:
		"mp3":
			var s := AudioStreamMP3.new()
			s.data = data
			return s
		"ogg":
			var s := AudioStreamOggVorbis.new()
			s.data = data
			return s
		"wav":
			var s := AudioStreamWAV.new()
			s.data = data
			return s
	return null


func _on_audio_stop() -> void:
	if _audio_player != null:
		_audio_player.stop()


## World settings ala SA-MP (paket 0x39 WORLD_SET): terapkan gravity,
## cuaca, dan jam dunia dari server ke scene Godot.
func _on_world_settings(gravity: float, weather: int, hour: int,
		minute: int) -> void:
	_world_gravity = gravity
	if player != null and _default_gravity > 0.0:
		# Skala relatif terhadap default GTA:SA 0.008.
		player.gravity = _default_gravity * (_world_gravity / 0.008)
	_apply_weather(weather)
	_apply_world_time(hour, minute)


# Preset warna langit sederhana per cuaca (id ala GTA:SA; 10 = Sunny).
func _apply_weather(w: int) -> void:
	if _sky_mat == null:
		return
	if w == 17:  # hujan (GTA:SA id 17)
		_sky_mat.sky_top_color = Color(0.22, 0.26, 0.33)
		_sky_mat.sky_horizon_color = Color(0.42, 0.46, 0.5)
		_sky_mat.ground_bottom_color = Color(0.12, 0.12, 0.14)
		_sky_mat.ground_horizon_color = Color(0.4, 0.43, 0.45)
	elif w in [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]:  # berawan/berkabut
		_sky_mat.sky_top_color = Color(0.45, 0.55, 0.7)
		_sky_mat.sky_horizon_color = Color(0.7, 0.73, 0.75)
		_sky_mat.ground_bottom_color = Color(0.16, 0.16, 0.17)
		_sky_mat.ground_horizon_color = Color(0.55, 0.57, 0.58)
	else:  # cerah (default: 10 = Sunny)
		_sky_mat.sky_top_color = Color(0.3, 0.55, 0.95)
		_sky_mat.sky_horizon_color = Color(0.72, 0.83, 0.95)
		_sky_mat.ground_bottom_color = Color(0.18, 0.18, 0.2)
		_sky_mat.ground_horizon_color = Color(0.6, 0.65, 0.7)


# Putar matahari mengikuti jam dunia: 06:00 terbit, 12:00 zenit,
# 18:00 terbenam; malam hari cahaya meredup.
func _apply_world_time(hour: int, minute: int) -> void:
	if _sun == null:
		return
	var h := float(hour) + float(minute) / 60.0
	var elev := sin((h - 6.0) / 12.0 * PI) * 75.0  # -75..75 derajat
	_sun.rotation_degrees = Vector3(-elev, -35, 0)
	var day_factor := clampf(elev / 60.0, 0.0, 1.0)
	_sun.light_energy = 0.08 + 1.07 * day_factor



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
