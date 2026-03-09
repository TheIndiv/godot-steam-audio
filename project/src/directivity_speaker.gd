## Attach as a child of a SteamAudioPlayer.
## Sets directivity properties on the parent and triggers periodic beeps
## only when the player is within proximity_radius.
extends Node

@export var dipole_weight: float = 0.0
@export var dipole_power: float = 1.0
@export var proximity_radius: float = 5.0
@export var beep_interval: float = 2.0

var _player_node: Node3D = null
var _audio: Node3D = null
var _timer: float = 0.0
var _next_beep: float = 0.0

func _ready() -> void:
	_audio = get_parent() as Node3D
	_audio.set("directivity", true)
	_audio.set("dipole_weight", dipole_weight)
	_audio.set("dipole_power", dipole_power)
	_next_beep = randf_range(0.3, beep_interval)

func _process(delta: float) -> void:
	if _player_node == null:
		var nodes := get_tree().get_nodes_in_group("player")
		if nodes.size() > 0:
			_player_node = nodes[0] as Node3D
		if _player_node == null:
			return

	var dist: float = _audio.global_position.distance_to(_player_node.global_position)
	if dist > proximity_radius:
		return

	_timer += delta
	if _timer >= _next_beep:
		_timer = 0.0
		_next_beep = beep_interval + randf_range(-0.4, 0.4)
		if not _audio.call("is_playing"):
			_audio.call("play")
