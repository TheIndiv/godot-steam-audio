extends Node3D

@onready var mode_label: RichTextLabel = $UI/ModeLabel

var speakers: Array = []
var directivity_on: bool = true

func _ready() -> void:
	speakers = [
		$Speakers/Speaker0,
		$Speakers/Speaker1,
		$Speakers/Speaker2,
		$Speakers/Speaker3,
	]
	_update_hud()

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo:
		if event.keycode == KEY_T:
			directivity_on = !directivity_on
			for spk in speakers:
				spk.set("directivity", directivity_on)
			_update_hud()

func _update_hud() -> void:
	if directivity_on:
		mode_label.text = "[b][T]  Directivity:  [color=#44ff88]ON[/color][/b]"
	else:
		mode_label.text = "[b][T]  Directivity:  [color=#ff6644]OFF  —  All speakers omnidirectional[/color][/b]"
