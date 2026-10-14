extends Node3D
## Kamera third-person: orbit dengan mouse, follow target yang
## dihaluskan, SpringArm3D mencegah kamera menembus gedung.

const MOUSE_SENS := 0.0028
const FOLLOW_LERP := 14.0

var target: Node3D
var yaw := 0.0
var pitch := -0.32

var _spring: SpringArm3D


func _ready() -> void:
	_spring = SpringArm3D.new()
	_spring.spring_length = 7.5
	_spring.margin = 0.4
	_spring.collision_mask = 1 # hanya dunia (pemain/mobil di layer 2)
	add_child(_spring)
	var cam := Camera3D.new()
	cam.far = 700.0
	cam.fov = 65.0
	_spring.add_child(cam)


func _input(event: InputEvent) -> void:
	if event is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		yaw -= event.relative.x * MOUSE_SENS
		pitch = clampf(pitch - event.relative.y * MOUSE_SENS, -1.15, 0.55)
	elif event is InputEventMouseButton and event.pressed:
		if Input.mouse_mode != Input.MOUSE_MODE_CAPTURED:
			Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
	elif event is InputEventKey and event.pressed and event.keycode == KEY_ESCAPE:
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE


func _process(delta: float) -> void:
	if target:
		var h: float = target.get("cam_height") if target.get("cam_height") else 1.6
		var tp: Vector3 = target.global_position + Vector3(0, h, 0)
		var t := 1.0 - exp(-FOLLOW_LERP * delta)
		global_position = global_position.lerp(tp, t)
	rotation.y = yaw
	rotation.x = pitch
