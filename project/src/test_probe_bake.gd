extends Node
## Headless smoke test for SteamAudioProbeVolume baking (Phase 2).
## Waits a frame so sibling geometry/config nodes have registered, bakes, checks the result,
## then quits with an exit code reflecting success/failure.
## Run with: Godot --headless --path project --quit-after 60 scenes/test_probe_bake.tscn

@export var probe_volume_path: NodePath

func _ready() -> void:
	await get_tree().process_frame
	await get_tree().process_frame

	var probe_volume: Node = get_node(probe_volume_path)
	var ok: bool = probe_volume.bake()
	var num_probes: int = probe_volume.get_num_probes()

	print("[probe_bake] bake() returned %s, num_probes=%d" % [ok, num_probes])
	if ok and num_probes > 0:
		print("[probe_bake] PASS")
		get_tree().quit(0)
	else:
		print("[probe_bake] FAIL")
		get_tree().quit(1)
