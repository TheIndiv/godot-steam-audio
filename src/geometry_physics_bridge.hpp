#ifndef STEAM_AUDIO_GEOMETRY_PHYSICS_BRIDGE_H
#define STEAM_AUDIO_GEOMETRY_PHYSICS_BRIDGE_H

#include "godot_cpp/classes/node3d.hpp"
#include "godot_cpp/classes/wrapped.hpp"
#include "material.hpp"
#include "phonon.h"
#include <vector>

using namespace godot;

// Auto-registers occlusion/reflection geometry from every StaticBody3D's CollisionShape3D
// under this node, instead of requiring a manually-placed SteamAudioGeometry per shape.
// A CollisionShape3D that already has its own SteamAudioGeometry child is left alone, so
// individual shapes can still opt out or use a different material.
class SteamAudioPhysicsSceneBridge : public Node3D {
	GDCLASS(SteamAudioPhysicsSceneBridge, Node3D);

private:
	std::vector<IPLStaticMesh> meshes;
	Ref<SteamAudioMaterial> mat;

	std::atomic<bool> created;
	std::atomic<bool> registered;

	void create_geometry();
	void destroy_geometry();
	void register_geometry();
	void unregister_geometry();

	void ready_internal();

protected:
	static void _bind_methods();

public:
	bool disabled = false;

	SteamAudioPhysicsSceneBridge();
	~SteamAudioPhysicsSceneBridge();
	void _notification(int p_what);

	void recalculate();
	Ref<SteamAudioMaterial> get_material();
	void set_material(Ref<SteamAudioMaterial> p_material);
	bool is_disabled() const { return disabled; }
	void set_disabled(bool p_disabled);
};

#endif // STEAM_AUDIO_GEOMETRY_PHYSICS_BRIDGE_H
