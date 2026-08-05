#ifndef STEAM_AUDIO_PROBE_VOLUME_INSPECTOR_H
#define STEAM_AUDIO_PROBE_VOLUME_INSPECTOR_H

#include "godot_cpp/classes/editor_inspector_plugin.hpp"
#include "godot_cpp/classes/label.hpp"
#include "godot_cpp/classes/node.hpp"

using namespace godot;

// Phase 2 item 2 (bake UI): a "Bake Probes" button + status label at the bottom of a
// SteamAudioProbeVolume's Inspector, since bake() otherwise has no editor affordance beyond a
// hand-written EditorScript. bake() is still synchronous/blocking (no progress signal yet) -
// the editor will visibly hang for the bake's duration, same as calling it any other way.
class SteamAudioProbeVolumeInspectorPlugin : public EditorInspectorPlugin {
	GDCLASS(SteamAudioProbeVolumeInspectorPlugin, EditorInspectorPlugin);

protected:
	static void _bind_methods();

public:
	bool _can_handle(Object *p_object) const override;
	void _parse_end(Object *p_object) override;

	void _on_bake_pressed(Node *p_volume, Label *p_status_label);
};

#endif // STEAM_AUDIO_PROBE_VOLUME_INSPECTOR_H
