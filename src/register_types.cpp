#include "register_types.hpp"

#include "config.hpp"
#include "geometry.hpp"
#include "geometry_dynamic.hpp"
#include "geometry_physics_bridge.hpp"
#include "godot_cpp/core/memory.hpp"
#include "listener.hpp"
#include "material.hpp"
#include "player.hpp"
#include "probe_volume.hpp"
#include "probe_volume_gizmo.hpp"
#include "server.hpp"
#include "stream.hpp"

#include <gdextension_interface.h>
#include <godot_cpp/classes/editor_plugin_registration.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

SteamAudioServer *srv;

void init_ext(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE && p_level != MODULE_INITIALIZATION_LEVEL_SERVERS &&
			p_level != MODULE_INITIALIZATION_LEVEL_EDITOR) {
		return;
	}

	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		ClassDB::register_class<SteamAudioStreamPlayback>();
		ClassDB::register_class<SteamAudioStream>();
		ClassDB::register_class<SteamAudioListener>();
		ClassDB::register_class<SteamAudioGeometry>();
		ClassDB::register_class<SteamAudioDynamicGeometry>();
		ClassDB::register_class<SteamAudioPhysicsSceneBridge>();
		ClassDB::register_class<SteamAudioProbeVolume>();
		ClassDB::register_class<SteamAudioMaterial>();
		ClassDB::register_class<SteamAudioConfig>();
		ClassDB::register_class<SteamAudioPlayer>();
	}

	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		GDREGISTER_CLASS(SteamAudioServer);
		srv = memnew(SteamAudioServer);
	}

	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		// Phase 5 (debug visualization) - editor-only, no runtime footprint.
		ClassDB::register_class<SteamAudioProbeVolumeGizmoPlugin>();
		ClassDB::register_class<SteamAudioEditorPlugin>();
		EditorPlugins::add_by_type<SteamAudioEditorPlugin>();
	}
}

void uninit_ext(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		// Tried calling srv->stop_all_players() here to sever any player AudioServer still
		// holds before our classes are torn down (godot-nexus-resonance#13 is the same
		// failure class: AudioServer calling back into a freed vtable at process exit).
		// Measured worse, not better (~80% of headless reload-smoke runs crashed vs ~40%
		// without it) - Node/AudioServer calls are apparently not safe from this callback
		// either. Left unimplemented; see NEXUS_RESONANCE_ROADMAP.md for what's been ruled out.
		//
		// Should call this to not leak, but thread->wait_for_finish() crashes...
		// the program is exiting anyway so I'm not too concerned
		// memdelete(srv);
	}

	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorPlugins::remove_by_type<SteamAudioEditorPlugin>();
	}
}

extern "C" {
GDExtensionBool GDE_EXPORT init_extension(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

	init_obj.register_initializer(init_ext);
	init_obj.register_terminator(uninit_ext);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
}
