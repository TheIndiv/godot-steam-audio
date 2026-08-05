#include "probe_volume_inspector.hpp"
#include "godot_cpp/classes/button.hpp"
#include "godot_cpp/classes/editor_interface.hpp"
#include "godot_cpp/classes/theme.hpp"
#include "godot_cpp/classes/v_box_container.hpp"
#include "probe_volume.hpp"

void SteamAudioProbeVolumeInspectorPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_bake_pressed", "volume", "status_label"), &SteamAudioProbeVolumeInspectorPlugin::_on_bake_pressed);
}

bool SteamAudioProbeVolumeInspectorPlugin::_can_handle(Object *p_object) const {
	return Object::cast_to<SteamAudioProbeVolume>(p_object) != nullptr;
}

void SteamAudioProbeVolumeInspectorPlugin::_parse_begin(Object *p_object) {
	auto *volume = Object::cast_to<SteamAudioProbeVolume>(p_object);
	if (volume == nullptr) {
		return;
	}

	auto *container = memnew(VBoxContainer);

	auto *button = memnew(Button);
	button->set_text("Bake Probes");
	// Same icon NavigationRegion3D's "Bake NavigationMesh" button uses - Steam Audio has no
	// icon of its own for this action, and this reads as the established "bake" convention.
	Ref<Theme> editor_theme = EditorInterface::get_singleton()->get_editor_theme();
	if (editor_theme.is_valid() && editor_theme->has_icon("Bake", "EditorIcons")) {
		button->set_button_icon(editor_theme->get_icon("Bake", "EditorIcons"));
	}
	container->add_child(button);

	auto *status_label = memnew(Label);
	int existing = volume->get_num_probes();
	status_label->set_text(existing > 0 ? String("Baked ") + String::num_int64(existing) + " probes this session." : "Not baked this session.");
	container->add_child(status_label);

	button->connect("pressed", Callable(this, "_on_bake_pressed").bind(volume, status_label));

	add_custom_control(container);
}

void SteamAudioProbeVolumeInspectorPlugin::_on_bake_pressed(Node *p_volume, Label *p_status_label) {
	auto *volume = Object::cast_to<SteamAudioProbeVolume>(p_volume);
	if (volume == nullptr) {
		return;
	}

	// Synchronous - see the header comment. The label only updates once this returns.
	p_status_label->set_text("Baking (editor will hang until done)...");
	bool ok = volume->bake();
	if (ok) {
		p_status_label->set_text(String("Baked ") + String::num_int64(volume->get_num_probes()) + " probes.");
	} else {
		p_status_label->set_text("Bake failed - see Output panel.");
	}
}
