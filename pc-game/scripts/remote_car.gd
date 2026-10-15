extends Node3D
## Mobil remote dari server (0x14 VEH_SPAWN / 0x13 VEH_SYNC / 0x15 VEH_DESPAWN).
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


func _ready() -> void:
	_body_mat = StandardMaterial3D.new()
	_body_mat.roughness = 0.55
	_body_mat.metallic = 0.25
	_add_box(Vector3(2.0, 0.62, 4.2), Vector3(0, 0.72, 0), _body_mat)
	_add_box(Vector3(1.7, 0.55, 2.1), Vector3(0, 1.28, 0.25),
		_make_mat(Color(0.08, 0.1, 0.14)))
	# Lampu depan.
	_add_box(Vector3(0.35, 0.18, 0.1), Vector3(-0.6, 0.85, -2.12),
		_make_mat(Color(1, 1, 0.9), true))
	_add_box(Vector3(0.35, 0.18, 0.1), Vector3(0.6, 0.85, -2.12),
		_make_mat(Color(1, 1, 0.9), true))
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


func setup(p_vid: int, p_model: int, x: float, y: float, z: float,
		angle_deg: float, c1: int, c2: int, p_health: int) -> void:
	vid = p_vid
	model = p_model
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
