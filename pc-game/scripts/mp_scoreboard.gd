extends CanvasLayer
## Scoreboard ala SA-MP (tahan TAB): kolom ID, nama, ping.

var _panel: PanelContainer
var _body: Label
var _hostname := ""


func _ready() -> void:
	layer = 8

	_panel = PanelContainer.new()
	_panel.set_anchors_preset(Control.PRESET_CENTER)
	_panel.custom_minimum_size = Vector2(430, 0)
	_panel.visible = false
	add_child(_panel)

	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_left", 18)
	margin.add_theme_constant_override("margin_right", 18)
	margin.add_theme_constant_override("margin_top", 14)
	margin.add_theme_constant_override("margin_bottom", 14)
	_panel.add_child(margin)

	var vbox := VBoxContainer.new()
	vbox.add_theme_constant_override("separation", 6)
	margin.add_child(vbox)

	var head := Label.new()
	head.name = "Head"
	head.text = ""
	head.add_theme_font_size_override("font_size", 20)
	head.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	vbox.add_child(head)

	_body = Label.new()
	_body.text = ""
	_body.add_theme_font_size_override("font_size", 16)
	_body.add_theme_constant_override("line_spacing", 3)
	vbox.add_child(_body)


func set_data(hostname: String, players: Array, own_id: int, own_ping: int) -> void:
	_hostname = hostname
	var head := _panel.get_node("MarginContainer/VBoxContainer/Head") as Label
	head.text = "%s  (%d pemain)" % [hostname, players.size()]
	var rows: Array[String] = ["%-4s  %-28s  %s" % ["ID", "Nama", "Ping"]]
	var sorted := players.duplicate()
	sorted.sort_custom(func(a: Dictionary, b: Dictionary) -> bool: return a["id"] < b["id"])
	for p in sorted:
		var ping_txt := str(own_ping) if int(p["id"]) == own_id else "-"
		rows.append("%-4d  %-28s  %s" % [int(p["id"]), str(p["name"]).left(28), ping_txt])
	_body.text = "\n".join(rows)


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.keycode == KEY_TAB:
		_panel.visible = event.pressed
		if event.pressed:
			get_viewport().set_input_as_handled()
