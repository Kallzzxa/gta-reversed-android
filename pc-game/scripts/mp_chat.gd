extends CanvasLayer
## Chat ala SA-MP: T buka input, Enter kirim, ESC batal. Riwayat 8 baris.

signal chat_sent(text: String)

const MAX_SHOW := 8
const MAX_KEEP := 40

var typing := false

var _log: RichTextLabel
var _input: LineEdit
var _lines: Array[String] = []


func _ready() -> void:
	layer = 5

	_log = RichTextLabel.new()
	_log.position = Vector2(16, 96)
	_log.size = Vector2(600, 230)
	_log.bbcode_enabled = true
	_log.scroll_following = true
	_log.add_theme_font_size_override("normal_font_size", 17)
	_log.add_theme_color_override("default_color", Color(1, 1, 1))
	_log.add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0.85))
	_log.add_theme_constant_override("shadow_offset_x", 1)
	_log.add_theme_constant_override("shadow_offset_y", 1)
	add_child(_log)

	_input = LineEdit.new()
	_input.position = Vector2(16, 330)
	_input.size = Vector2(600, 34)
	_input.max_length = 128
	_input.visible = false
	_input.text_submitted.connect(_on_submit)
	add_child(_input)


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo:
		if event.keycode == KEY_T and not typing and visible:
			open()
			get_viewport().set_input_as_handled()
		elif event.keycode == KEY_ESCAPE and typing:
			close()
			get_viewport().set_input_as_handled()


func open() -> void:
	typing = true
	_input.visible = true
	_input.text = ""
	_input.grab_focus()
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE


func close() -> void:
	typing = false
	_input.visible = false
	_input.release_focus()
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED


func _on_submit(text: String) -> void:
	var t := text.strip_edges()
	close()
	if t != "":
		chat_sent.emit(t)


func add_message(kind: int, text: String) -> void:
	var color := "white"
	match kind:
		0:
			color = "white"
		1:
			color = "#ffd34d"
		2:
			color = "#c58cff"
		3:
			color = "#ffe14d"
		4:
			color = "#9aa0a6"
	_lines.append("[color=%s]%s[/color]" % [color, text.xml_escape()])
	while _lines.size() > MAX_KEEP:
		_lines.pop_front()
	_log.text = "\n".join(_lines.slice(maxi(0, _lines.size() - MAX_SHOW)))
