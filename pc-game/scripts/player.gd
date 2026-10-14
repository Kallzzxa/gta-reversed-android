extends CharacterBody3D
## Karakter third-person: WASD jalan, Shift lari, Space lompat,
## E masuk mobil di dekatnya. Gerak relatif terhadap arah kamera.

const WALK_SPEED := 6.0
const RUN_SPEED := 11.0
const JUMP_VEL := 7.5
const TURN_LERP := 12.0

var gravity: float = ProjectSettings.get_setting("physics/3d/default_gravity")
var cars: Array = []
var rig: Node3D
var in_car := false
var cam_height := 1.6

var _e_prev := false
var _body: MeshInstance3D
var _head: MeshInstance3D
var _col: CollisionShape3D


func _ready() -> void:
	collision_layer = 2
	collision_mask = 1
	_col = CollisionShape3D.new()
	var cap := CapsuleShape3D.new()
	cap.radius = 0.38
	cap.height = 1.75
	_col.shape = cap
	_col.position = Vector3(0, 0.9, 0)
	add_child(_col)

	_body = MeshInstance3D.new()
	var bm := CapsuleMesh.new()
	bm.radius = 0.38
	bm.height = 1.75
	_body.mesh = bm
	_body.position = Vector3(0, 0.9, 0)
	var mat := StandardMaterial3D.new()
	mat.albedo_color = Color(0.15, 0.35, 0.75)
	mat.roughness = 0.8
	_body.material_override = mat
	add_child(_body)

	_head = MeshInstance3D.new()
	var sm := SphereMesh.new()
	sm.radius = 0.26
	sm.height = 0.52
	_head.mesh = sm
	_head.position = Vector3(0, 1.95, 0)
	var skin := StandardMaterial3D.new()
	skin.albedo_color = Color(0.92, 0.72, 0.55)
	skin.roughness = 0.8
	_head.material_override = skin
	add_child(_head)


func nearest_car(max_dist := 4.5) -> CharacterBody3D:
	var best: CharacterBody3D = null
	var best_d := max_dist
	for c in cars:
		if c.driver_in:
			continue
		var d := global_position.distance_to(c.global_position)
		if d < best_d:
			best_d = d
			best = c
	return best


func enter_car(car: CharacterBody3D) -> void:
	in_car = true
	visible = false
	_col.disabled = true
	car.driver_in = true
	if rig:
		rig.target = car


func exit_car() -> void:
	var car := _my_car()
	in_car = false
	visible = true
	_col.disabled = false
	if car:
		var side: Vector3 = car.global_transform.basis.x.normalized()
		global_position = car.global_position + side * 2.6 + Vector3(0, 1.0, 0)
		velocity = Vector3.ZERO
		car.driver_in = false
	if rig:
		rig.target = self


func _my_car() -> CharacterBody3D:
	for c in cars:
		if c.driver_in:
			return c
	return null


func _physics_process(delta: float) -> void:
	if in_car:
		var car := _my_car()
		if car:
			global_position = car.global_position + Vector3(0, 0.6, 0)
		return

	var input_dir := Vector2.ZERO
	if Input.is_physical_key_pressed(KEY_W):
		input_dir.y -= 1.0
	if Input.is_physical_key_pressed(KEY_S):
		input_dir.y += 1.0
	if Input.is_physical_key_pressed(KEY_A):
		input_dir.x -= 1.0
	if Input.is_physical_key_pressed(KEY_D):
		input_dir.x += 1.0

	var sprint := Input.is_physical_key_pressed(KEY_SHIFT)
	var speed := RUN_SPEED if sprint else WALK_SPEED
	var yaw: float = rig.yaw if rig else rotation.y
	var dir := Basis(Vector3.UP, yaw) * Vector3(input_dir.x, 0.0, input_dir.y)
	if dir.length() > 0.01:
		dir = dir.normalized()
		rotation.y = lerp_angle(rotation.y, atan2(-dir.x, -dir.z), TURN_LERP * delta)

	velocity.x = dir.x * speed
	velocity.z = dir.z * speed
	if is_on_floor():
		if Input.is_physical_key_pressed(KEY_SPACE):
			velocity.y = JUMP_VEL
		elif velocity.y < 0.0:
			velocity.y = -1.0
	else:
		velocity.y -= gravity * delta
	move_and_slide()

	# E: masuk mobil (edge-trigger).
	var e := Input.is_physical_key_pressed(KEY_E)
	if e and not _e_prev:
		var car := nearest_car()
		if car:
			enter_car(car)
	_e_prev = e
