#include "probe_volume_gizmo.hpp"
#include "godot_cpp/classes/standard_material3d.hpp"
#include "probe_volume.hpp"

// ---------------------------------------------------------------------------------------------
// SteamAudioProbeVolumeGizmoPlugin
// ---------------------------------------------------------------------------------------------

void SteamAudioProbeVolumeGizmoPlugin::_bind_methods() {}

SteamAudioProbeVolumeGizmoPlugin::SteamAudioProbeVolumeGizmoPlugin() {
	create_material("steam_audio_probe_volume_box", Color(0.2f, 0.8f, 1.0f));
	create_material("steam_audio_probe_volume_points", Color(1.0f, 0.5f, 0.1f));
}

bool SteamAudioProbeVolumeGizmoPlugin::_has_gizmo(Node3D *p_for_node_3d) const {
	return Object::cast_to<SteamAudioProbeVolume>(p_for_node_3d) != nullptr;
}

String SteamAudioProbeVolumeGizmoPlugin::_get_gizmo_name() const {
	return "SteamAudioProbeVolume";
}

void SteamAudioProbeVolumeGizmoPlugin::_redraw(const Ref<EditorNode3DGizmo> &p_gizmo) {
	p_gizmo->clear();

	auto *volume = Object::cast_to<SteamAudioProbeVolume>(p_gizmo->get_node_3d());
	if (volume == nullptr) {
		return;
	}

	Vector3 half = volume->get_size() * 0.5f;
	Vector3 corners[8] = {
		Vector3(-half.x, -half.y, -half.z),
		Vector3(half.x, -half.y, -half.z),
		Vector3(half.x, -half.y, half.z),
		Vector3(-half.x, -half.y, half.z),
		Vector3(-half.x, half.y, -half.z),
		Vector3(half.x, half.y, -half.z),
		Vector3(half.x, half.y, half.z),
		Vector3(-half.x, half.y, half.z),
	};
	static const int edges[12][2] = {
		{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
		{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
		{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
	};

	PackedVector3Array box_lines;
	for (const auto &edge : edges) {
		box_lines.push_back(corners[edge[0]]);
		box_lines.push_back(corners[edge[1]]);
	}
	p_gizmo->add_lines(box_lines, get_material("steam_audio_probe_volume_box", p_gizmo));

	PackedVector3Array probes_world = volume->get_probe_positions();
	if (!probes_world.is_empty()) {
		Transform3D world_to_local = volume->get_global_transform().affine_inverse();
		const float r = 0.15f;
		PackedVector3Array probe_lines;
		for (int i = 0; i < probes_world.size(); ++i) {
			Vector3 p = world_to_local.xform(probes_world[i]);
			probe_lines.push_back(p + Vector3(r, 0, 0));
			probe_lines.push_back(p - Vector3(r, 0, 0));
			probe_lines.push_back(p + Vector3(0, r, 0));
			probe_lines.push_back(p - Vector3(0, r, 0));
			probe_lines.push_back(p + Vector3(0, 0, r));
			probe_lines.push_back(p - Vector3(0, 0, r));
		}
		p_gizmo->add_lines(probe_lines, get_material("steam_audio_probe_volume_points", p_gizmo));
	}
}

// ---------------------------------------------------------------------------------------------
// SteamAudioEditorPlugin
// ---------------------------------------------------------------------------------------------

void SteamAudioEditorPlugin::_bind_methods() {}

void SteamAudioEditorPlugin::_enter_tree() {
	probe_volume_gizmo_plugin.instantiate();
	add_node_3d_gizmo_plugin(probe_volume_gizmo_plugin);

	probe_volume_inspector_plugin.instantiate();
	add_inspector_plugin(probe_volume_inspector_plugin);
}

void SteamAudioEditorPlugin::_exit_tree() {
	remove_node_3d_gizmo_plugin(probe_volume_gizmo_plugin);
	probe_volume_gizmo_plugin.unref();

	remove_inspector_plugin(probe_volume_inspector_plugin);
	probe_volume_inspector_plugin.unref();
}
