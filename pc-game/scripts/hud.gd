extends CanvasLayer
## HUD sederhana: judul, kecepatan mobil, dan petunjuk kontrol.

var _player: CharacterBody3D
var _cars: Array = []
var _speed_label: Label
var _hint_label: Label
var _mp_label: Label


func bind(player: CharacterBody3D, cars: Array) -> void:
	_player = player
	_cars = cars


## Status koneksi multiplayer (dipanggil main.gd), mis. "● Online | 3 pemain | 42ms".
func set_mp_status(text: String) -> void:
	_mp_label.text = text


func _ready() -> void:
	var title := Label.new()
	title.text = "RO-MP PC"
	title.position = Vector2(16, 12)
	title.add_theme_font_size_override("font_size", 28)
	title.add_theme_color_override("font_color", Color(1, 1, 1))
	title.add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0.8))
	title.add_theme_constant_override("shadow_offset_x", 2)
	title.add_theme_constant_override("shadow_offset_y", 2)
	add_child(title)

	_speed_label = Label.new()
	_speed_label.position = Vector2(16, 52)
	_speed_label.add_theme_font_size_override("font_size", 22)
	_speed_label.add_theme_color_override("font_color", Color(1, 1, 1))
	_speed_label.add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0.8))
	_speed_label.add_theme_constant_override("shadow_offset_x", 2)
	_speed_label.add_theme_constant_override("shadow_offset_y", 2)
	add_child(_speed_label)

	_hint_label = Label.new()
	_hint_label.position = Vector2(16, 680)
	_hint_label.add_theme_font_size_override("font_size", 16)
	_hint_label.add_theme_color_override("font_color", Color(1, 1, 1, 0.92))
	_hint_label.add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0.8))
	_hint_label.add_theme_constant_override("shadow_offset_x", 1)
	_hint_label.add_theme_constant_override("shadow_offset_y", 1)
	add_child(_hint_label)

	_mp_label = Label.new()
	_mp_label.set_anchors_preset(Control.PRESET_TOP_RIGHT)
	_mp_label.position = Vector2(-316, 12)
	_mp_label.size = Vector2(300, 30)
	_mp_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	_mp_label.text = "○ Offline"
	_mp_label.add_theme_font_size_override("font_size", 18)
	_mp_label.add_theme_color_override("font_color", Color(1, 1, 1, 0.9))
	_mp_label.add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0.8))
	_mp_label.add_theme_constant_override("shadow_offset_x", 1)
	_mp_label.add_theme_constant_override("shadow_offset_y", 1)
	add_child(_mp_label)


func _process(_delta: float) -> void:
	if _player == null:
		return
	var driving := false
	for c in _cars:
		if c.driver_in:
			driving = true
			_speed_label.visible = true
			_speed_label.text = "%d km/h" % int(absf(c.speed) * 3.6)
	if not driving:
		_speed_label.visible = false

	if _player.in_car:
		_hint_label.text = "W/S gas & rem | A/D setir | Space handbrake | F keluar mobil | ESC lepas mouse"
	else:
		var near := _player.nearest_car() != null
		if near:
			_hint_label.text = "WASD jalan | Shift lari | Space lompat | E masuk mobil | ESC lepas mouse"
		else:
			_hint_label.text = "WASD jalan | Shift lari | Space lompat | dekati mobil lalu tekan E | ESC lepas mouse"
