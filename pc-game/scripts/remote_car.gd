extends Node3D
## Mobil remote dari server (0x14 VEH_SPAWN / 0x13 VEH_SYNC / 0x15 VEH_DESPAWN /
## 0x3A-0x3C komponen & paintjob ala SA-MP).
## Render + interpolasi; tanpa physics/input.

const LERP_RATE := 10.0
const SNAP_DIST := 25.0

var vid := -1
var model := 0
var health := 1000
var has_target := false

var _target_pos := Vector3.ZERO
var _target_heading := 0.0
var _body_mat: StandardMaterial3D
var _smoke: Label3D
var _spoiler: Node3D  # visual spoiler (slot 0) — v1: kotak sederhana
var _components: Dictionary = {}  # slot -> compid
var _last_c1 := 0
var _last_c2 := 0
var _headlight_mats: Array = []  # material lampu depan (untuk params lights)
var _param_tag: Label3D  # indikator 🔒/🎯 dari params doors/objective
var _params: Dictionary = {}  # engine/lights/alarm/doors/bonnet/boot/objective


func _ready() -> void:
	_body_mat = StandardMaterial3D.new()
	_body_mat.roughness = 0.55
	_body_mat.metallic = 0.25
	_add_box(Vector3(2.0, 0.62, 4.2), Vector3(0, 0.72, 0), _body_mat)
	_add_box(Vector3(1.7, 0.55, 2.1), Vector3(0, 1.28, 0.25),
		_make_mat(Color(0.08, 0.1, 0.14)))
	# Lampu depan.
	var hl1 := _make_mat(Color(1, 1, 0.9), true)
	var hl2 := _make_mat(Color(1, 1, 0.9), true)
	_headlight_mats = [hl1, hl2]
	_add_box(Vector3(0.35, 0.18, 0.1), Vector3(-0.6, 0.85, -2.12), hl1)
	_add_box(Vector3(0.35, 0.18, 0.1), Vector3(0.6, 0.85, -2.12), hl2)
	# Roda (statis untuk remote).
	for sx in [-1.0, 1.0]:
		for sz in [-1.35, 1.35]:
			var w := MeshInstance3D.new()
			var cm := CylinderMesh.new()
			cm.top_radius = 0.38
			cm.bottom_radius = 0.38
			cm.height = 0.32
			w.mesh = cm
			w.rotation.z = PI * 0.5
			w.position = Vector3(sx * 0.98, 0.38, sz)
			w.material_override = _make_mat(Color(0.08, 0.08, 0.08))
			add_child(w)

	_smoke = Label3D.new()
	_smoke.text = ""
	_smoke.position = Vector3(0, 2.2, 0)
	_smoke.font_size = 96
	_smoke.pixel_size = 0.01
	_smoke.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	_smoke.no_depth_test = true
	add_child(_smoke)

	# Spoiler sederhana (slot 0); disembunyikan sampai ada komponen spoiler.
	_spoiler = Node3D.new()
	_spoiler.position = Vector3(0, 1.55, 2.0)
	var blade := MeshInstance3D.new()
	var bm := BoxMesh.new()
	bm.size = Vector3(1.8, 0.08, 0.5)
	blade.mesh = bm
	blade.material_override = _make_mat(Color(0.12, 0.12, 0.14))
	_spoiler.add_child(blade)
	for sx in [-0.7, 0.7]:
		var leg := MeshInstance3D.new()
		var lm := BoxMesh.new()
		lm.size = Vector3(0.08, 0.45, 0.08)
		leg.mesh = lm
		leg.position = Vector3(sx, -0.25, 0.1)
		leg.material_override = _make_mat(Color(0.12, 0.12, 0.14))
		_spoiler.add_child(leg)
	_spoiler.visible = false
	add_child(_spoiler)

	# Tag indikator params ala SA-MP (🔒 pintu terkunci, 🎯 objective).
	_param_tag = Label3D.new()
	_param_tag.text = ""
	_param_tag.position = Vector3(0, 2.9, 0)
	_param_tag.font_size = 96
	_param_tag.pixel_size = 0.01
	_param_tag.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	_param_tag.no_depth_test = true
	add_child(_param_tag)


func set_component(slot: int, compid: int) -> void:
	# 0x3A VEH_COMP — v1: hanya slot 0 (spoiler) yang divisualkan.
	_components[slot] = compid
	if slot == 0 and _spoiler != null:
		_spoiler.visible = true


func remove_component(compid: int) -> void:
	# 0x3B VEH_COMP_REMOVE.
	for s in _components.keys():
		if _components[s] == compid:
			_components.erase(s)
			if s == 0 and _spoiler != null:
				_spoiler.visible = false
			break


func set_paintjob(paintjob: int) -> void:
	# 0x3C VEH_PAINTJOB ala SA-MP ChangeVehiclePaintjob.
	# v1: paintjob mengganti saturasi warna bodi sebagai penanda visual.
	if paintjob == 255:
		_body_mat.albedo_color = _samp_color(_last_c1, _last_c2)
		return
	var base := _samp_color(_last_c1, _last_c2)
	_body_mat.albedo_color = Color.from_hsv(
		base.h + float(paintjob) * 0.33, minf(base.s + 0.2, 1.0),
		minf(base.v + 0.15, 1.0))


func set_params(engine: int, lights: int, _alarm: int, doors: int,
		_bonnet: int, _boot: int, objective: int) -> void:
	# 0x3D VEH_PARAMS ala SA-MP SetVehicleParamsEx (-1 = jangan ubah).
	var vals := {"engine": engine, "lights": lights, "doors": doors,
		"objective": objective}
	for k in vals.keys():
		if vals[k] != -1:
			_params[k] = vals[k]
	if lights != -1:
		for m in _headlight_mats:
			m.emission_enabled = lights == 1
	_refresh_param_tag()


func set_params_for_player(_pid: int, objective: int, doorslocked: int) -> void:
	# 0x3E VEH_PARAMS_P ala SA-MP SetVehicleParamsForPlayer.
	if objective != -1:
		_params["objective"] = objective
	if doorslocked != -1:
		_params["doors"] = doorslocked
	_refresh_param_tag()


func _refresh_param_tag() -> void:
	var t := ""
	if _params.get("doors", -1) == 1:
		t += "🔒"
	if _params.get("objective", -1) == 1:
		t += "🎯"
	if _params.get("engine", -1) == 0:
		t += "🚫"
	_param_tag.text = t


func setup(p_vid: int, p_model: int, x: float, y: float, z: float,
		angle_deg: float, c1: int, c2: int, p_health: int) -> void:
	vid = p_vid
	model = p_model
	_last_c1 = c1
	_last_c2 = c2
	_body_mat.albedo_color = _samp_color(c1, c2)
	set_sync(x, y, z, angle_deg, p_health, true)


func set_sync(x: float, y: float, z: float, heading_deg: float, p_health: int,
		snap := false) -> void:
	health = p_health
	var p := Vector3(x, y, z)
	var h := deg_to_rad(heading_deg)
	if not has_target or snap or global_position.distance_to(p) > SNAP_DIST:
		global_position = p
		rotation.y = h
		has_target = true
	_target_pos = p
	_target_heading = h
	# Indikator rusak ala SA-MP (health <= 250 = hancur).
	_smoke.text = "🔥" if health <= 250 else ""


func _process(delta: float) -> void:
	if not has_target:
		return
	var t := minf(1.0, delta * LERP_RATE)
	global_position = global_position.lerp(_target_pos, t)
	rotation.y = lerp_angle(rotation.y, _target_heading, t)


func _make_mat(color: Color, emissive := false) -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	m.albedo_color = color
	m.roughness = 0.55
	m.metallic = 0.25
	if emissive:
		m.emission_enabled = true
		m.emission = color
		m.emission_energy_multiplier = 2.5
	return m


func _add_box(size: Vector3, pos: Vector3, mat: Material) -> void:
	var mi := MeshInstance3D.new()
	var bm := BoxMesh.new()
	bm.size = size
	mi.mesh = bm
	mi.position = pos
	mi.material_override = mat
	add_child(mi)


## Warna SA-MP (id 0-255) -> Color. Beberapa warna umum dipetakan langsung,
## sisanya hash HSV agar stabil.
func _samp_color(c1: int, _c2: int) -> Color:
	match c1:
		0:
			return Color(0.05, 0.05, 0.05)
		1:
			return Color(0.95, 0.95, 0.95)
		3:
			return Color(0.75, 0.12, 0.1)
		6:
			return Color(0.95, 0.75, 0.1)
		79:
			return Color(0.1, 0.25, 0.7)
	return Color.from_hsv(fmod(float(c1) * 0.137, 1.0), 0.65, 0.75)
