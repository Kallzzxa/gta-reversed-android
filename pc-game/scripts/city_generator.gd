extends Node3D
## Generator kota prosedural ala GTA: jalan grid, trotoar, gedung,
## lampu jalan, pohon, dan marka jalan. Semua collision ikut dibuat.

const BLOCKS := 5            # jumlah blok per sisi
const BLOCK_SIZE := 60.0     # lebar blok (meter)
const ROAD_WIDTH := 14.0     # lebar jalan (meter)
const PITCH := BLOCK_SIZE + ROAD_WIDTH

var _mats := {}


func _mat(color: Color, emissive := false) -> StandardMaterial3D:
	var key := color.to_html() + ("e" if emissive else "")
	if _mats.has(key):
		return _mats[key]
	var m := StandardMaterial3D.new()
	m.albedo_color = color
	m.roughness = 0.92
	if emissive:
		m.emission_enabled = true
		m.emission = color
		m.emission_energy_multiplier = 2.0
	_mats[key] = m
	return m


## Kotak visual saja (tanpa collision).
func _deco_box(size: Vector3, pos: Vector3, color: Color, emissive := false) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	var bm := BoxMesh.new()
	bm.size = size
	mi.mesh = bm
	mi.material_override = _mat(color, emissive)
	mi.position = pos
	add_child(mi)
	return mi


## Kotak solid: mesh + StaticBody3D.
func _solid_box(size: Vector3, pos: Vector3, color: Color) -> StaticBody3D:
	var body := StaticBody3D.new()
	body.position = pos
	var cs := CollisionShape3D.new()
	var shape := BoxShape3D.new()
	shape.size = size
	cs.shape = shape
	body.add_child(cs)
	var mi := MeshInstance3D.new()
	var bm := BoxMesh.new()
	bm.size = size
	mi.mesh = bm
	mi.material_override = _mat(color)
	body.add_child(mi)
	add_child(body)
	return body


func _cylinder(radius: float, height: float, pos: Vector3, color: Color, collide := false) -> Node3D:
	var mi := MeshInstance3D.new()
	var cm := CylinderMesh.new()
	cm.top_radius = radius
	cm.bottom_radius = radius
	cm.height = height
	mi.mesh = cm
	mi.material_override = _mat(color)
	if collide:
		var body := StaticBody3D.new()
		body.position = pos
		var cs := CollisionShape3D.new()
		var shape := CylinderShape3D.new()
		shape.radius = radius
		shape.height = height
		cs.shape = shape
		body.add_child(cs)
		body.add_child(mi)
		mi.position = Vector3.ZERO
		add_child(body)
		return body
	mi.position = pos
	add_child(mi)
	return mi


func _sphere(radius: float, pos: Vector3, color: Color, emissive := false) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	var sm := SphereMesh.new()
	sm.radius = radius
	sm.height = radius * 2.0
	mi.mesh = sm
	mi.material_override = _mat(color, emissive)
	mi.position = pos
	add_child(mi)
	return mi


func generate() -> void:
	randomize()
	var half := float(BLOCKS) * PITCH * 0.5

	# Tanah dasar (rumput).
	_solid_box(Vector3(half * 2.0 + 240.0, 1.0, half * 2.0 + 240.0),
		Vector3(0, -0.5, 0), Color(0.3, 0.42, 0.27))

	var road_y := 0.06
	var asphalt := Color(0.16, 0.16, 0.17)

	# Jalan: BLOCKS+1 strip vertikal + horizontal.
	for i in range(BLOCKS + 1):
		var c := -half + float(i) * PITCH
		_deco_box(Vector3(ROAD_WIDTH, 0.12, half * 2.0), Vector3(c, road_y, 0), asphalt)
		_deco_box(Vector3(half * 2.0, 0.12, ROAD_WIDTH), Vector3(0, road_y, c), asphalt)
		# Marka tengah putus-putus.
		var z := -half
		while z < half:
			_deco_box(Vector3(0.35, 0.02, 3.0), Vector3(c, road_y + 0.07, z), Color(0.85, 0.75, 0.2))
			_deco_box(Vector3(3.0, 0.02, 0.35), Vector3(z, road_y + 0.07, c), Color(0.85, 0.75, 0.2))
			z += 9.0

	# Blok: trotoar + gedung + pohon.
	var palet := [
		Color(0.62, 0.6, 0.58), Color(0.55, 0.42, 0.35), Color(0.45, 0.55, 0.68),
		Color(0.7, 0.68, 0.62), Color(0.5, 0.5, 0.55), Color(0.58, 0.5, 0.42),
	]
	for bx in range(BLOCKS):
		for bz in range(BLOCKS):
			var x0 := -half + float(bx) * PITCH + ROAD_WIDTH * 0.5
			var z0 := -half + float(bz) * PITCH + ROAD_WIDTH * 0.5
			var cx := x0 + BLOCK_SIZE * 0.5
			var cz := z0 + BLOCK_SIZE * 0.5
			# Trotoar.
			_deco_box(Vector3(BLOCK_SIZE, 0.24, BLOCK_SIZE),
				Vector3(cx, 0.12, cz), Color(0.52, 0.52, 0.53))
			# 2x2 gedung per blok.
			for gx in range(2):
				for gz in range(2):
					var w := randf_range(14.0, 22.0)
					var d := randf_range(14.0, 22.0)
					var h := randf_range(9.0, 38.0)
					var px := x0 + 8.0 + float(gx) * (BLOCK_SIZE * 0.5) + randf_range(-2.0, 2.0)
					var pz := z0 + 8.0 + float(gz) * (BLOCK_SIZE * 0.5) + randf_range(-2.0, 2.0)
					var col: Color = palet[randi() % palet.size()]
					_solid_box(Vector3(w, h, d), Vector3(px, 0.24 + h * 0.5, pz), col)
					# Lis atap.
					_deco_box(Vector3(w + 0.8, 0.5, d + 0.8),
						Vector3(px, 0.24 + h + 0.25, pz), col.darkened(0.25))
			# Pohon di sudut trotoar.
			if randf() < 0.7:
				var tx := x0 + randf_range(4.0, BLOCK_SIZE - 4.0)
				var tz := z0 + randf_range(4.0, BLOCK_SIZE - 4.0)
				_cylinder(0.28, 3.2, Vector3(tx, 1.8, tz), Color(0.35, 0.25, 0.15), true)
				_sphere(1.9, Vector3(tx, 4.6, tz), Color(0.2, 0.45, 0.22))
				_sphere(1.3, Vector3(tx + 0.9, 3.9, tz + 0.5), Color(0.24, 0.5, 0.25))

	# Lampu jalan di sepanjang tiap jalan.
	var lamp_x := Color(0.25, 0.25, 0.27)
	for i in range(BLOCKS + 1):
		var c := -half + float(i) * PITCH
		var t := -half + 15.0
		var side := 1.0
		while t < half:
			_lamp(Vector3(c + side * (ROAD_WIDTH * 0.5 + 1.5), 0, t), lamp_x)
			_lamp(Vector3(t, 0, c - side * (ROAD_WIDTH * 0.5 + 1.5)), lamp_x)
			side = -side
			t += 34.0


func _lamp(base: Vector3, color: Color) -> void:
	_cylinder(0.14, 7.0, base + Vector3(0, 3.5, 0), color, true)
	_sphere(0.42, base + Vector3(0, 7.2, 0), Color(1.0, 0.95, 0.75), true)
