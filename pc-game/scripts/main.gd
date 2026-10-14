extends Node3D
## RO-MP PC — entry point.
## Membangun seluruh dunia dari kode: langit, kota, pemain, mobil, kamera, HUD.

const CityGenerator = preload("res://scripts/city_generator.gd")
const PlayerScript = preload("res://scripts/player.gd")
const CarScript = preload("res://scripts/car.gd")
const CameraRigScript = preload("res://scripts/camera_rig.gd")
const HudScript = preload("res://scripts/hud.gd")

var player: CharacterBody3D
var cars: Array = []
var camera_rig: Node3D
var hud: CanvasLayer


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

	# Mobil: 1 dekat spawn + 2 parkir di jalan.
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

	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED


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
