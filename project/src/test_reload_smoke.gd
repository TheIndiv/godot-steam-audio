extends Node
## Headless regression test for scene reload/teardown races (upstream #102/#75).
## Repeatedly loads and frees the demo scene as a child, forcing SteamAudioPlayer/
## SteamAudioListener/SteamAudioGeometry through add/remove while the reflection
## thread is active. A crash or hang here is a regression; a clean exit is a pass.
## Run with: Godot --headless --path project --quit-after 1200 scenes/test_reload_smoke.tscn

const RELOAD_SCENE_PATH := "res://scenes/demo.tscn"
const RELOAD_CYCLES := 20
const FRAMES_PER_CYCLE := 10

var cycle := 0
var frame_in_cycle := 0
var loaded_scene: Node = null
var finishing := false

func _ready() -> void:
	print("[reload_smoke] starting: %d cycles, %d frames/cycle" % [RELOAD_CYCLES, FRAMES_PER_CYCLE])
	_spawn_cycle()

func _process(_delta: float) -> void:
	if finishing:
		# One frame's grace so the final queue_free() actually flushes before quit().
		print("[reload_smoke] PASS: %d cycles completed without crash" % RELOAD_CYCLES)
		get_tree().quit(0)
		return

	frame_in_cycle += 1
	if frame_in_cycle < FRAMES_PER_CYCLE:
		return

	_teardown_cycle()
	cycle += 1
	if cycle >= RELOAD_CYCLES:
		finishing = true
		return

	frame_in_cycle = 0
	_spawn_cycle()

func _spawn_cycle() -> void:
	print("[reload_smoke] cycle %d/%d: loading" % [cycle + 1, RELOAD_CYCLES])
	var packed: PackedScene = load(RELOAD_SCENE_PATH)
	loaded_scene = packed.instantiate()
	add_child(loaded_scene)

func _teardown_cycle() -> void:
	print("[reload_smoke] cycle %d/%d: tearing down" % [cycle + 1, RELOAD_CYCLES])
	if loaded_scene != null:
		loaded_scene.queue_free()
		loaded_scene = null
