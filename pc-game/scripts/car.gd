extends CharacterBody3D
## Mobil arcade yang bisa dikendarai: W/S gas-rem-mundur,
## A/D setir, Space handbrake, F keluar mobil.

const ACCEL := 20.0
const BRAKE := 34.0
const MAX_SPEED := 38.0
const MAX_REVERSE := 12.0
const TURN_RATE := 2.0
const DRAG := 9.0

var gravity: float = ProjectSettings.get_setting("physics/3d/default_gravity")
var body_color := Color(0.75, 0.12, 0.1)
var player: CharacterBody3D
var driver_in := false
var speed := 0.0          # m/s, negatif = mundur
var cam_height := 2.6

var _wheels: Array[MeshInstance3D] = []
var _f_prev := false


func _ready() -> void:
	collision_layer = 2
	collision_mask = 1

	var col := CollisionShape3D.new()
	var box := BoxShape3D.new()
	box.size = Vector3(2.0, 1.1, 4.2)
	col.shape = box
	col.position = Vector3(0, 0.85, 0)
	add_child(col)

	_add_box(Vector3(2.0, 0.62, 4.2), Vector3(0, 0.72, 0), body_color)
	_add_box(Vector3(1.7, 0.55, 2.1), Vector3(0, 1.28, 0.25), Color(0.08, 0.1, 0.14))
	# Lampu depan.
	_add_box(Vector3(0.35, 0.18, 0.1), Vector3(-0.6, 0.85, -2.12), Color(1, 1, 0.9), true)
	_add_box(Vector3(0.35, 0.18, 0.1), Vector3(0.6, 0.85, -2.12), Color(1, 1, 0.9), true)
	# Roda.
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
			var wm := StandardMaterial3D.new()
			wm.albedo_color = Color(0.08, 0.08, 0.08)
			wm.roughness = 0.95
			w.material_override = wm
			add_child(w)
			_wheels.append(w)


func _add_box(size: Vector3, pos: Vector3, color: Color, emissive := false) -> void:
	var mi := MeshInstance3D.new()
	var bm := BoxMesh.new()
	bm.size = size
	mi.mesh = bm
	mi.position = pos
	var m := StandardMaterial3D.new()
	m.albedo_color = color
	m.roughness = 0.55
	m.metallic = 0.25
	if emissive:
		m.emission_enabled = true
		m.emission = color
		m.emission_energy_multiplier = 2.5
	mi.material_override = m
	add_child(mi)


func _physics_process(delta: float) -> void:
	if not driver_in:
		speed = move_toward(speed, 0.0, DRAG * delta)
	if driver_in:
		var throttle := 0.0
		if Input.is_physical_key_pressed(KEY_W):
			throttle += 1.0
		if Input.is_physical_key_pressed(KEY_S):
			throttle -= 1.0
		if throttle > 0.0:
			speed = minf(speed + ACCEL * delta, MAX_SPEED)
		elif throttle < 0.0:
			if speed > 0.8:
				speed = maxf(speed - BRAKE * delta, 0.0)
			else:
				speed = maxf(speed - ACCEL * 0.65 * delta, -MAX_REVERSE)
		else:
			speed = move_toward(speed, 0.0, DRAG * delta)
		if Input.is_physical_key_pressed(KEY_SPACE):
			speed = move_toward(speed, 0.0, 42.0 * delta)

		var steer := 0.0
		if Input.is_physical_key_pressed(KEY_A):
			steer += 1.0
		if Input.is_physical_key_pressed(KEY_D):
			steer -= 1.0
		var spd_factor := clampf(absf(speed) / MAX_SPEED, 0.0, 1.0)
		var dir_sign := 1.0 if speed >= 0.0 else -1.0
		if absf(speed) > 0.4:
			rotation.y += steer * TURN_RATE * (1.25 - spd_factor * 0.7) * dir_sign * delta

		# F: keluar (edge-trigger).
		var f := Input.is_physical_key_pressed(KEY_F)
		if f and not _f_prev and player:
			player.exit_car()
		_f_prev = f

	var fwd := -global_transform.basis.z
	fwd.y = 0.0
	fwd = fwd.normalized()
	velocity.x = fwd.x * speed
	velocity.z = fwd.z * speed
	if is_on_floor():
		velocity.y = -1.0
	else:
		velocity.y -= gravity * delta
	move_and_slide()

	# Putar roda sesuai kecepatan.
	for w in _wheels:
		w.rotate_x(speed * delta / 0.38)
