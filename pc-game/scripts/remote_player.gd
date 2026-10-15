extends CharacterBody3D
## Ped pemain remote: kapsul + kepala + nametag 3D, interpolasi posisi 10Hz.

const LERP_RATE := 10.0
const SNAP_DIST := 15.0

var player_name := "?"
var is_npc := false
var has_target := false

var _target_pos := Vector3.ZERO
var _target_heading := 0.0
var _nametag: Label3D


func _ready() -> void:
	collision_layer = 2
	collision_mask = 1

	var col := CollisionShape3D.new()
	var cap := CapsuleShape3D.new()
	cap.radius = 0.38
	cap.height = 1.75
	col.shape = cap
	col.position = Vector3(0, 0.9, 0)
	add_child(col)

	var body := MeshInstance3D.new()
	var bm := CapsuleMesh.new()
	bm.radius = 0.38
	bm.height = 1.75
	body.mesh = bm
	body.position = Vector3(0, 0.9, 0)
	var mat := StandardMaterial3D.new()
	mat.albedo_color = Color(0.75, 0.45, 0.15)
	mat.roughness = 0.8
	body.material_override = mat
	add_child(body)

	var head := MeshInstance3D.new()
	var sm := SphereMesh.new()
	sm.radius = 0.26
	sm.height = 0.52
	head.mesh = sm
	head.position = Vector3(0, 1.95, 0)
	var skin := StandardMaterial3D.new()
	skin.albedo_color = Color(0.92, 0.72, 0.55)
	skin.roughness = 0.8
	head.material_override = skin
	add_child(head)

	_nametag = Label3D.new()
	_nametag.text = ("[BOT] " if is_npc else "") + player_name
	_nametag.position = Vector3(0, 2.55, 0)
	_nametag.font_size = 64
	_nametag.pixel_size = 0.008
	_nametag.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	_nametag.no_depth_test = true
	_nametag.modulate = Color(0.65, 0.85, 1.0) if is_npc else Color(1, 1, 1)
	_nametag.outline_size = 12
	_nametag.outline_modulate = Color(0, 0, 0, 0.9)
	add_child(_nametag)


func set_target(x: float, y: float, z: float, heading_deg: float) -> void:
	var p := Vector3(x, y, z)
	var h := deg_to_rad(heading_deg)
	if not has_target or global_position.distance_to(p) > SNAP_DIST:
		# Spawn pertama atau lompatan jauh (teleport): snap, jangan lerp.
		global_position = p
		rotation.y = h
		has_target = true
	_target_pos = p
	_target_heading = h


func _process(delta: float) -> void:
	if not has_target:
		return
	var t := minf(1.0, delta * LERP_RATE)
	global_position = global_position.lerp(_target_pos, t)
	rotation.y = lerp_angle(rotation.y, _target_heading, t)
