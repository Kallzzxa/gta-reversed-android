extends CanvasLayer
## Menu koneksi multiplayer: nickname, host, port, password + tombol Connect.
## Muncul saat game dibuka; ESC/tombol "Main Offline" menutupnya.

signal connect_requested(nickname: String, host: String, port: int, password: String)
signal offline_requested()

var _panel: PanelContainer
var _nick: LineEdit
var _host: LineEdit
var _port: LineEdit
var _pw: LineEdit
var _status: Label
var _connect_btn: Button


func _ready() -> void:
	layer = 10

	var dim := ColorRect.new()
	dim.color = Color(0, 0, 0, 0.72)
	dim.set_anchors_preset(Control.PRESET_FULL_RECT)
	add_child(dim)

	_panel = PanelContainer.new()
	_panel.set_anchors_preset(Control.PRESET_CENTER)
	_panel.custom_minimum_size = Vector2(380, 0)
	add_child(_panel)

	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_left", 24)
	margin.add_theme_constant_override("margin_right", 24)
	margin.add_theme_constant_override("margin_top", 20)
	margin.add_theme_constant_override("margin_bottom", 20)
	_panel.add_child(margin)

	var vbox := VBoxContainer.new()
	vbox.add_theme_constant_override("separation", 10)
	margin.add_child(vbox)

	var title := Label.new()
	title.text = "RO-MP Multiplayer"
	title.add_theme_font_size_override("font_size", 26)
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	vbox.add_child(title)

	_nick = _field(vbox, "Nickname", "Pemain")
	_host = _field(vbox, "Server IP", "127.0.0.1")
	_port = _field(vbox, "Port", "7777")
	_pw = _field(vbox, "Password (opsional)", "")
	_pw.secret = true

	_connect_btn = Button.new()
	_connect_btn.text = "Connect"
	_connect_btn.custom_minimum_size = Vector2(0, 44)
	_connect_btn.pressed.connect(_on_connect)
	vbox.add_child(_connect_btn)

	var offline_btn := Button.new()
	offline_btn.text = "Main Offline"
	offline_btn.pressed.connect(func() -> void: offline_requested.emit())
	vbox.add_child(offline_btn)

	_status = Label.new()
	_status.text = ""
	_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_status.add_theme_font_size_override("font_size", 15)
	_status.add_theme_color_override("font_color", Color(1, 0.85, 0.4))
	vbox.add_child(_status)

	# Enter di field mana pun = Connect.
	for f in [_nick, _host, _port, _pw]:
		f.text_submitted.connect(func(_t: String) -> void: _on_connect())


func _field(parent: Control, label_text: String, placeholder: String) -> LineEdit:
	var lab := Label.new()
	lab.text = label_text
	lab.add_theme_font_size_override("font_size", 15)
	parent.add_child(lab)
	var le := LineEdit.new()
	le.placeholder_text = placeholder
	le.custom_minimum_size = Vector2(0, 34)
	parent.add_child(le)
	return le


func _on_connect() -> void:
	var nick := _nick.text.strip_edges()
	if nick == "":
		nick = _nick.placeholder_text
	var host := _host.text.strip_edges()
	if host == "":
		host = _host.placeholder_text
	var port := int(_port.text.strip_edges()) if _port.text.strip_edges().is_valid_int() else 7777
	port = clampi(port, 1, 65535)
	_status.text = "Menghubungkan ke %s:%d ..." % [host, port]
	_connect_btn.disabled = true
	connect_requested.emit(nick, host, port, _pw.text)


func set_status(text: String, is_error := false) -> void:
	_status.text = text
	_status.add_theme_color_override("font_color",
		Color(1, 0.45, 0.4) if is_error else Color(1, 0.85, 0.4))
	_connect_btn.disabled = false


func show_menu() -> void:
	visible = true
	_status.text = ""
	_connect_btn.disabled = false
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE


func hide_menu() -> void:
	visible = false
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
