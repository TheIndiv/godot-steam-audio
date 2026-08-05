#include "geometry_physics_bridge.hpp"
#include "geometry.hpp"
#include "geometry_common.hpp"
#include "godot_cpp/classes/collision_shape3d.hpp"
#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/static_body3d.hpp"
#include "godot_cpp/variant/typed_array.hpp"
#include "phonon.h"
#include "server.hpp"

void SteamAudioPhysicsSceneBridge::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_material"), &SteamAudioPhysicsSceneBridge::get_material);
	ClassDB::bind_method(D_METHOD("set_material", "p_material"), &SteamAudioPhysicsSceneBridge::set_material);
	ClassDB::bind_method(D_METHOD("is_disabled"), &SteamAudioPhysicsSceneBridge::is_disabled);
	ClassDB::bind_method(D_METHOD("set_disabled", "p_disabled"), &SteamAudioPhysicsSceneBridge::set_disabled);
	ClassDB::bind_method(D_METHOD("recalculate"), &SteamAudioPhysicsSceneBridge::recalculate);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "disabled"), "set_disabled", "is_disabled");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "SteamAudioMaterial"), "set_material", "get_material");
}

SteamAudioPhysicsSceneBridge::SteamAudioPhysicsSceneBridge() {
	created.store(false);
	registered.store(false);
}

SteamAudioPhysicsSceneBridge::~SteamAudioPhysicsSceneBridge() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	if (!disabled) {
		unregister_geometry();
	}
	destroy_geometry();
}

void SteamAudioPhysicsSceneBridge::ready_internal() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	create_geometry();
	if (!disabled) {
		register_geometry();
	}
}

void SteamAudioPhysicsSceneBridge::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			// Unlike SteamAudioGeometry (which only reads its already-in-tree parent), this
			// node walks its own descendants, so it must wait until the whole subtree has
			// entered the tree - at NOTIFICATION_ENTER_TREE, this node's children haven't
			// entered yet (parent-first propagation), so get_global_transform() on them fails.
			ready_internal();
			break;
		case NOTIFICATION_EXIT_TREE:
			unregister_geometry();
			break;
	}
}

void SteamAudioPhysicsSceneBridge::set_disabled(bool p_disabled) {
	if (disabled == p_disabled) {
		return;
	}

	if (p_disabled) {
		unregister_geometry();
	} else {
		register_geometry();
	}

	disabled = p_disabled;
}

void SteamAudioPhysicsSceneBridge::recalculate() {
	unregister_geometry();
	destroy_geometry();
	create_geometry();
	register_geometry();
}

void SteamAudioPhysicsSceneBridge::create_geometry() {
	if (created.load()) {
		return;
	}

	created.store(true);

	auto scene = SteamAudioServer::get_singleton()->get_global_state()->scene;
	TypedArray<Node> shapes = find_children("*", "CollisionShape3D", true, false);

	for (int i = 0; i < shapes.size(); i++) {
		auto shape = Object::cast_to<CollisionShape3D>(shapes[i]);
		if (shape == nullptr) {
			continue;
		}
		if (!Object::cast_to<StaticBody3D>(shape->get_parent())) {
			// Only static level geometry is picked up automatically - bodies that move
			// (RigidBody3D, CharacterBody3D, Area3D, ...) need SteamAudioDynamicGeometry or a
			// manually-managed SteamAudioGeometry instead.
			continue;
		}
		if (!shape->find_children("*", "SteamAudioGeometry", false, false).is_empty()) {
			// Already has its own geometry node - respect the override (different material,
			// or an explicit opt-out) instead of double-registering it.
			continue;
		}

		auto shape_meshes = create_meshes_from_coll_inst_3d(shape, scene, mat);
		meshes.insert(meshes.end(), shape_meshes.begin(), shape_meshes.end());
	}
}

void SteamAudioPhysicsSceneBridge::destroy_geometry() {
	if (!created.load()) {
		return;
	}

	created.store(false);

	for (auto &mesh : meshes) {
		iplStaticMeshRelease(&mesh);
	}
	meshes.clear();
}

void SteamAudioPhysicsSceneBridge::register_geometry() {
	if (registered.load()) {
		return;
	}

	registered.store(true);

	for (auto mesh : meshes) {
		SteamAudioServer::get_singleton()->add_static_mesh(mesh);
	}
}

void SteamAudioPhysicsSceneBridge::unregister_geometry() {
	if (!registered.load()) {
		return;
	}

	registered.store(false);

	for (auto ipl_mesh : meshes) {
		SteamAudioServer::get_singleton()->remove_static_mesh(ipl_mesh);
	}
}

Ref<SteamAudioMaterial> SteamAudioPhysicsSceneBridge::get_material() { return mat; }
void SteamAudioPhysicsSceneBridge::set_material(Ref<SteamAudioMaterial> p_material) { mat = p_material; }
