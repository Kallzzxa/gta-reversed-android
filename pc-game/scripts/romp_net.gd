extends Node
## Klien protokol RO-MP (TCP) untuk Godot.
## Dipakai: const RompNet = preload("res://scripts/romp_net.gd")
##          var net = RompNet.new(); add_child(net)
##          net.connected.connect(...); net.connect_to("127.0.0.1", 7777, "Nick")
##
## Lihat server/PROTOCOL.md untuk format paket. Semua integer little-endian.

# --- Tipe paket ---
const T_HELLO := 0x01
const T_WELCOME := 0x02
const T_REJECT := 0x03
const T_POS := 0x10
const T_PPOS := 0x11
const T_VEH := 0x12
const T_VEH_SYNC := 0x13
const T_VEH_SPAWN := 0x14
const T_VEH_DESPAWN := 0x15
const T_VEH_SEAT := 0x16
const T_WPN_SYNC := 0x18
const T_CHAT := 0x20
const T_CHAT_MSG := 0x21
const T_JOIN := 0x22
const T_LEAVE := 0x23
const T_LIST := 0x24
const T_INT_VW := 0x25
const T_POS_SET := 0x27
const T_LABEL_SPAWN := 0x28
const T_LABEL_DESPAWN := 0x29
const T_LABEL_TEXT := 0x2A
const T_LABEL_POS := 0x2B
const T_OBJECT_SPAWN := 0x2C
const T_OBJECT_DESPAWN := 0x2D
const T_OBJECT_MOVE := 0x2E
const T_OBJECT_UPDATE := 0x2F
const T_PING := 0x30
const T_PONG := 0x31
const T_ACTOR_SPAWN := 0x32
const T_ACTOR_DESPAWN := 0x33
const T_ACTOR_POS := 0x34
const T_ACTOR_ANIM := 0x35
const T_ACTOR_HEALTH := 0x36
const T_AUDIO_PLAY := 0x37
const T_AUDIO_STOP := 0x38
const T_WORLD_SET := 0x39
const T_VEH_COMP := 0x3A
const T_VEH_COMP_REMOVE := 0x3B
const T_VEH_PAINTJOB := 0x3C
const T_VEH_PARAMS := 0x3D
const T_VEH_PARAMS_P := 0x3E
const T_ATTACH_OBJ := 0x3F
const T_NUI := 0x40

const VER := 1
const MAX_FRAME := 4096

# --- Kind chat (0x21) ---
const CHAT_NORMAL := 0
const CHAT_INFO := 1
const CHAT_ME := 2
const CHAT_PM := 3
const CHAT_JOINLEAVE := 4

# --- State pemain (0x10) ---
const STATE_ONFOOT := 0
const STATE_DRIVER := 1
const STATE_PASSENGER := 2

signal connected(playerid: int, hostname: String, motd: String, maxplayers: int)
signal connect_failed(reason: String)
signal disconnected()
signal player_list(players: Array)
signal player_joined(playerid: int, name: String, is_npc: bool)
signal player_left(playerid: int, reason: int)
signal player_pos(playerid: int, x: float, y: float, z: float, heading: float, state: int, vehicle_model: int)
signal chat_msg(sender_id: int, kind: int, text: String)
signal vehicle_spawn(vid: int, model: int, x: float, y: float, z: float, angle: float, c1: int, c2: int, health: int, driverid: int)
signal vehicle_despawn(vid: int)
signal vehicle_sync(driverid: int, vid: int, x: float, y: float, z: float, heading: float, health: int)
signal vehicle_seat(vid: int, seat: int, playerid: int)
signal weapon_shot(shooterid: int, weaponid: int, hittype: int, hitid: int, x: float, y: float, z: float)
signal int_vw(interior: int, virtual_world: int)
signal teleport(x: float, y: float, z: float)
signal label_spawn(labelid: int, attach_type: int, attach_id: int, x: float, y: float, z: float, drawdist: float, color: int, text: String)
signal label_despawn(labelid: int)
signal label_text(labelid: int, color: int, text: String)
signal label_pos(labelid: int, x: float, y: float, z: float)
signal object_spawn(objid: int, modelid: int, x: float, y: float, z: float, rx: float, ry: float, rz: float, drawdist: float, vw: int)
signal object_despawn(objid: int)
signal object_move(objid: int, tx: float, ty: float, tz: float, speed: float, trx: float, try_: float, trz: float)
signal object_update(objid: int, x: float, y: float, z: float, rx: float, ry: float, rz: float)
signal actor_spawn(actorid: int, modelid: int, x: float, y: float, z: float, angle: float, health: int, invulnerable: bool, vw: int)
signal actor_despawn(actorid: int)
signal actor_pos(actorid: int, x: float, y: float, z: float, angle: float)
signal actor_anim(actorid: int, anim: String)
signal actor_health(actorid: int, health: int)
signal audio_play(url: String, usepos: bool, x: float, y: float, z: float, distance: float, loop: bool)
signal audio_stop()
signal world_settings(gravity: float, weather: int, hour: int, minute: int)
signal vehicle_component(vid: int, compid: int, slot: int)
signal vehicle_component_remove(vid: int, compid: int)
signal vehicle_paintjob(vid: int, paintjob: int)
signal vehicle_params(vid: int, engine: int, lights: int, alarm: int, doors: int, bonnet: int, boot: int, objective: int)
signal vehicle_params_p(vid: int, pid: int, objective: int, doorslocked: int)
signal attach_obj_set(pid: int, index: int, modelid: int, bone: int, off: Vector3, rot: Vector3, scale: Vector3)
signal attach_obj_remove(pid: int, index: int)
signal nui_message(json_text: String)
signal ping_updated(ms: int)

var player_id := -1
var is_connected := false
## Interval ping client->server (detik). Protokol menyarankan 15; bisa
## dikecilkan untuk test.
var ping_interval := 15.0

var _tcp := StreamPeerTCP.new()
var _inbuf := PackedByteArray()
var _want_hello := false
var _nick := ""
var _pw := ""
var _ping_tick := 0
var _ping_sent_msec := 0
var _ping_timer := 0.0


## Pembaca paket: cursor di atas StreamPeerBuffer little-endian.
class Reader:
	var _spb := StreamPeerBuffer.new()

	func _init(data: PackedByteArray) -> void:
		_spb.big_endian = false
		_spb.data_array = data

	func u8() -> int:
		return _spb.get_u8()

	## Baca int8 bertanda (-1 = unset ala SA-MP SetVehicleParamsEx).
	func i8() -> int:
		var v := _spb.get_u8()
		return v - 256 if v >= 128 else v

	func u16() -> int:
		return _spb.get_u16()

	func u32() -> int:
		return _spb.get_u32()

	func f32() -> float:
		return _spb.get_float()

	func str8() -> String:
		return _spb.get_string(_spb.get_u8())

	func str16() -> String:
		return _spb.get_string(_spb.get_u16())


## Penulis paket.
class Writer:
	var _spb := StreamPeerBuffer.new()

	func _init() -> void:
		_spb.big_endian = false

	func u8(v: int) -> void:
		_spb.put_u8(v)

	func u16(v: int) -> void:
		_spb.put_u16(v)

	func u32(v: int) -> void:
		_spb.put_u32(v)

	func f32(v: float) -> void:
		_spb.put_float(v)

	func str8(s: String) -> void:
		var b := s.to_utf8_buffer()
		_spb.put_u8(mini(b.size(), 255))
		_spb.put_data(b.slice(0, mini(b.size(), 255)))

	func frame(ftype: int) -> PackedByteArray:
		var payload := _spb.data_array
		var out := StreamPeerBuffer.new()
		out.big_endian = false
		out.put_u16(payload.size() + 1)
		out.put_u8(ftype)
		out.put_data(payload)
		return out.data_array


func connect_to(host: String, port: int, nickname: String, password: String = "") -> void:
	disconnect_now()
	_nick = nickname.left(24)
	_pw = password
	_want_hello = true
	_inbuf.clear()
	var err := _tcp.connect_to_host(host, port)
	if err != OK:
		_want_hello = false
		connect_failed.emit("connect_to_host gagal (%s)" % err)


func disconnect_now() -> void:
	is_connected = false
	_want_hello = false
	player_id = -1
	_ping_sent_msec = 0
	_ping_timer = 0.0
	_inbuf.clear()
	_tcp.disconnect_from_host()


func send_pos(x: float, y: float, z: float, heading: float, state: int, vehicle_model: int) -> void:
	if not is_connected:
		return
	var w := Writer.new()
	w.f32(x)
	w.f32(y)
	w.f32(z)
	w.f32(heading)
	w.u8(state)
	w.u16(vehicle_model)
	_send(T_POS, w)


func send_chat(text: String) -> void:
	if not is_connected:
		return
	var b := text.to_utf8_buffer()
	if b.size() == 0 or b.size() > 128:
		return
	var w := Writer.new()
	w.str8(text)
	_send(T_CHAT, w)


## Sync kendaraan sebagai sopir (0x12). Server mengabaikan bila bukan sopir.
func send_vehicle(vid: int, x: float, y: float, z: float, heading: float, health: int) -> void:
	if not is_connected:
		return
	var w := Writer.new()
	w.u16(vid)
	w.f32(x)
	w.f32(y)
	w.f32(z)
	w.f32(heading)
	w.u16(clampi(health, 0, 1000))
	_send(T_VEH, w)


func _process(delta: float) -> void:
	_tcp.poll()
	var st := _tcp.get_status()
	match st:
		StreamPeerTCP.STATUS_CONNECTED:
			if not is_connected and _want_hello:
				is_connected = true
				_send_hello()
			if is_connected:
				_drain()
				_ping_timer += delta
				if _ping_timer >= ping_interval:
					_ping_timer = 0.0
					_send_ping()
		StreamPeerTCP.STATUS_ERROR, StreamPeerTCP.STATUS_NONE:
			if is_connected or _want_hello:
				_drop("koneksi terputus")


func _send_hello() -> void:
	var w := Writer.new()
	w.u8(VER)
	w.str8(_nick)
	w.str8(_pw)
	_send(T_HELLO, w)


func _send_ping() -> void:
	_ping_tick += 1
	_ping_sent_msec = Time.get_ticks_msec()
	var w := Writer.new()
	w.u32(_ping_tick)
	_send(T_PING, w)


func _reply_pong(tick: int) -> void:
	var w := Writer.new()
	w.u32(tick)
	_send(T_PONG, w)


func _send(ftype: int, w: Writer) -> void:
	var err := _tcp.put_data(w.frame(ftype))
	if err != OK:
		_drop("kirim gagal")


func _drop(reason: String) -> void:
	var was := is_connected or _want_hello
	disconnect_now()
	if was:
		disconnected.emit()


func _drain() -> void:
	var avail := _tcp.get_available_bytes()
	if avail <= 0:
		return
	var res := _tcp.get_data(avail)
	if res[0] != OK:
		_drop("baca gagal")
		return
	_inbuf.append_array(res[1])
	_parse()


func _parse() -> void:
	while _inbuf.size() >= 2:
		var ln := _inbuf[0] | (_inbuf[1] << 8)
		if ln < 1 or ln > MAX_FRAME:
			_drop("frame rusak")
			return
		if _inbuf.size() < 2 + ln:
			return
		var pkt := _inbuf.slice(2, 2 + ln)
		_inbuf = _inbuf.slice(2 + ln)
		_dispatch(pkt[0], pkt.slice(1))


func _dispatch(ftype: int, payload: PackedByteArray) -> void:
	var r := Reader.new(payload)
	match ftype:
		T_WELCOME:
			player_id = r.u16()
			var hostname := r.str8()
			var motd := r.str8()
			var maxp := r.u8()
			_ping_timer = 0.0
			connected.emit(player_id, hostname, motd, maxp)
		T_REJECT:
			var reason := r.str8()
			disconnect_now()
			connect_failed.emit(reason)
		T_PPOS:
			var pid := r.u16()
			player_pos.emit(pid, r.f32(), r.f32(), r.f32(), r.f32(), r.u8(), r.u16())
		T_VEH_SYNC:
			var driverid := r.u16()
			var vid := r.u16()
			vehicle_sync.emit(driverid, vid, r.f32(), r.f32(), r.f32(), r.f32(), r.u16())
		T_VEH_SPAWN:
			var vid := r.u16()
			var model := r.u16()
			var x := r.f32()
			var y := r.f32()
			var z := r.f32()
			var angle := r.f32()
			var c1 := r.u8()
			var c2 := r.u8()
			var health := r.u16()
			var driverid := r.u16()
			vehicle_spawn.emit(vid, model, x, y, z, angle, c1, c2, health, driverid)
		T_VEH_DESPAWN:
			vehicle_despawn.emit(r.u16())
		T_VEH_SEAT:
			var vid := r.u16()
			var seat := r.u8()
			vehicle_seat.emit(vid, seat, r.u16())
		T_VEH_COMP:
			vehicle_component.emit(r.u16(), r.u16(), r.u8())
		T_VEH_COMP_REMOVE:
			vehicle_component_remove.emit(r.u16(), r.u16())
		T_VEH_PAINTJOB:
			vehicle_paintjob.emit(r.u16(), r.u8())
		T_VEH_PARAMS:
			vehicle_params.emit(r.u16(), r.i8(), r.i8(), r.i8(), r.i8(),
				r.i8(), r.i8(), r.i8())
		T_VEH_PARAMS_P:
			vehicle_params_p.emit(r.u16(), r.u16(), r.i8(), r.i8())
		T_ATTACH_OBJ:
			var atpid := r.u8()
			var atindex := r.u8()
			var atmodel := r.u32()
			if atmodel == 0:
				attach_obj_remove.emit(atpid, atindex)
			else:
				var atbone := r.u8()
				var atoff := Vector3(r.f32(), r.f32(), r.f32())
				var atrot := Vector3(r.f32(), r.f32(), r.f32())
				var atscl := Vector3(r.f32(), r.f32(), r.f32())
				attach_obj_set.emit(atpid, atindex, atmodel, atbone,
					atoff, atrot, atscl)
		T_WPN_SYNC:
			var shooter := r.u16()
			var weapon := r.u16()
			var hittype := r.u8()
			var hitid := r.u16()
			weapon_shot.emit(shooter, weapon, hittype, hitid, r.f32(), r.f32(), r.f32())
		T_CHAT_MSG:
			var sender := r.u16()
			var kind := r.u8()
			chat_msg.emit(sender, kind, r.str8())
		T_JOIN:
			var pid := r.u16()
			var pname := r.str8()
			var flags := r.u8() if r._spb.get_available_bytes() > 0 else 0
			player_joined.emit(pid, pname, (flags & 1) != 0)
		T_LIST:
			var count := r.u8()
			var arr: Array = []
			for i in range(count):
				arr.append({"id": r.u16(), "name": r.str8()})
			player_list.emit(arr)
		T_LEAVE:
			player_left.emit(r.u16(), r.u8())
		T_INT_VW:
			int_vw.emit(r.u8(), r.u32())
		T_POS_SET:
			teleport.emit(r.f32(), r.f32(), r.f32())
		T_LABEL_SPAWN:
			var lid := r.u16()
			var atype := r.u8()
			var aid := r.u16()
			var lx := r.f32()
			var ly := r.f32()
			var lz := r.f32()
			var ldd := r.f32()
			var lcolor := r.u32()
			var ltext := r.str8()
			label_spawn.emit(lid, atype, aid, lx, ly, lz, ldd, lcolor, ltext)
		T_LABEL_DESPAWN:
			label_despawn.emit(r.u16())
		T_LABEL_TEXT:
			label_text.emit(r.u16(), r.u32(), r.str8())
		T_LABEL_POS:
			label_pos.emit(r.u16(), r.f32(), r.f32(), r.f32())
		T_OBJECT_SPAWN:
			var oid := r.u16()
			var omodel := r.u32()
			var ox := r.f32()
			var oy := r.f32()
			var oz := r.f32()
			var orx := r.f32()
			var ory := r.f32()
			var orz := r.f32()
			var odd := r.f32()
			var ovw := r.u32()
			if ovw >= 0x80000000:
				ovw -= 0x100000000  # i32: -1 = semua world
			object_spawn.emit(oid, omodel, ox, oy, oz, orx, ory, orz, odd, ovw)
		T_OBJECT_DESPAWN:
			object_despawn.emit(r.u16())
		T_OBJECT_MOVE:
			object_move.emit(r.u16(), r.f32(), r.f32(), r.f32(), r.f32(),
				r.f32(), r.f32(), r.f32())
		T_OBJECT_UPDATE:
			object_update.emit(r.u16(), r.f32(), r.f32(), r.f32(),
				r.f32(), r.f32(), r.f32())
		T_ACTOR_SPAWN:
			var aid := r.u16()
			var amodel := r.u16()
			var ax := r.f32()
			var ay := r.f32()
			var az := r.f32()
			var aangle := r.f32()
			var ahealth := r.u16()
			var ainv := r.u8() != 0
			var avw := r.u32()
			actor_spawn.emit(aid, amodel, ax, ay, az, aangle, ahealth, ainv, avw)
		T_ACTOR_DESPAWN:
			actor_despawn.emit(r.u16())
		T_ACTOR_POS:
			actor_pos.emit(r.u16(), r.f32(), r.f32(), r.f32(), r.f32())
		T_ACTOR_ANIM:
			actor_anim.emit(r.u16(), r.str8())
		T_ACTOR_HEALTH:
			actor_health.emit(r.u16(), r.u16())
		T_AUDIO_PLAY:
			var aurl := r.str16()
			var ausepos := r.u8() != 0
			var ax := r.f32()
			var ay := r.f32()
			var az := r.f32()
			var adist := r.f32()
			var aloop := r.u8() != 0
			audio_play.emit(aurl, ausepos, ax, ay, az, adist, aloop)
		T_AUDIO_STOP:
			audio_stop.emit()
		T_WORLD_SET:
			var wgravity := r.f32()
			var wweather := r.u8()
			var whour := r.u8()
			var wminute := r.u8()
			world_settings.emit(wgravity, wweather, whour, wminute)
		T_PING:
			_reply_pong(r.u32())
		T_PONG:
			var tick := r.u32()
			if tick == _ping_tick and _ping_sent_msec > 0:
				var ms := int(Time.get_ticks_msec()) - _ping_sent_msec
				_ping_sent_msec = 0
				ping_updated.emit(clampi(ms, 0, 9999))
		T_NUI:
			nui_message.emit(r.str16())
