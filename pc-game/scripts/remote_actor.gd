extends CharacterBody3D
## Actor statis ala SA-MP (CreateActor): ped statis server-side.
## Render sederhana: kapsul hijau + tag model + animasi aktif.

var actor_id := 0
var modelid := 0
var health := 100
var invulnerable := false
var current_anim := ""

var _tag: Label3D
var _body_mat: StandardMaterial3D


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
	_body_mat = StandardMaterial3D.new()
	_body_mat.albedo_color = Color(0.25, 0.75, 0.35)
	_body_mat.roughness = 0.8
	body.material_override = _body_mat
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

	_tag = Label3D.new()
	_tag.position = Vector3(0, 2.55, 0)
	_tag.font_size = 64
	_tag.pixel_size = 0.008
	_tag.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	_tag.no_depth_test = true
	_tag.modulate = Color(0.5, 1.0, 0.55)
	_tag.outline_size = 12
	_tag.outline_modulate = Color(0, 0, 0, 0.9)
	add_child(_tag)
	_refresh_tag()


func setup(aid: int, amodel: int, x: float, y: float, z: float,
		angle: float, ahealth: int, ainv: bool) -> void:
	actor_id = aid
	modelid = amodel
	health = ahealth
	invulnerable = ainv
	global_position = Vector3(x, y, z)
	rotation.y = deg_to_rad(angle)
	_refresh_tag()
	_refresh_body()


func set_pos(x: float, y: float, z: float, angle: float) -> void:
	# Actor statis: snap langsung (tidak ada interpolasi periodik).
	global_position = Vector3(x, y, z)
	rotation.y = deg_to_rad(angle)


func set_anim(anim: String) -> void:
	current_anim = anim
	_refresh_tag()


func set_health(ahealth: int) -> void:
	health = ahealth
	_refresh_tag()
	_refresh_body()


func _refresh_tag() -> void:
	if not is_instance_valid(_tag):
		return
	var s := "[ACTOR %d]" % modelid
	if invulnerable:
		s += " [KEBAL]"
	if current_anim != "":
		s += "\n" + current_anim
	_tag.text = s


func _refresh_body() -> void:
	if _body_mat == null:
		return
	# Mati (health 0): warna abu-abu gelap.
	_body_mat.albedo_color = Color(0.35, 0.35, 0.38) if health <= 0 \
		else Color(0.25, 0.75, 0.35)
