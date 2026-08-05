#ifndef STEAM_AUDIO_PROBE_VOLUME_GIZMO_H
#define STEAM_AUDIO_PROBE_VOLUME_GIZMO_H

#include "godot_cpp/classes/editor_node3d_gizmo.hpp"
#include "godot_cpp/classes/editor_node3d_gizmo_plugin.hpp"
#include "godot_cpp/classes/editor_plugin.hpp"
#include "probe_volume_inspector.hpp"

using namespace godot;

// Phase 5 (debug visualization): draws a SteamAudioProbeVolume's box extents, always, and
// each probe position from its last bake() call (see SteamAudioProbeVolume::get_probe_positions,
// only populated in-session - a disk-only load still shows just the box). This is the tool that
// would have made the Phase 2 transform bugs (see PROGRESS_NOTES.md) visible in the editor
// directly instead of needing a throwaway marker-mesh test scene.
class SteamAudioProbeVolumeGizmoPlugin : public EditorNode3DGizmoPlugin {
	GDCLASS(SteamAudioProbeVolumeGizmoPlugin, EditorNode3DGizmoPlugin);

protected:
	static void _bind_methods();

public:
	SteamAudioProbeVolumeGizmoPlugin();

	bool _has_gizmo(Node3D *p_for_node_3d) const override;
	String _get_gizmo_name() const override;
	void _redraw(const Ref<EditorNode3DGizmo> &p_gizmo) override;
};

// Registers SteamAudioProbeVolumeGizmoPlugin with the 3D editor viewport.
class SteamAudioEditorPlugin : public EditorPlugin {
	GDCLASS(SteamAudioEditorPlugin, EditorPlugin);

private:
	Ref<SteamAudioProbeVolumeGizmoPlugin> probe_volume_gizmo_plugin;
	Ref<SteamAudioProbeVolumeInspectorPlugin> probe_volume_inspector_plugin;

protected:
	static void _bind_methods();

public:
	void _enter_tree() override;
	void _exit_tree() override;
};

#endif // STEAM_AUDIO_PROBE_VOLUME_GIZMO_H
