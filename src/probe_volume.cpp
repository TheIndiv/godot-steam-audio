#include "probe_volume.hpp"
#include "config.hpp"
#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/file_access.hpp"
#include "phonon.h"
#include "server.hpp"
#include "steam_audio.hpp"
#include <cstring>

void SteamAudioProbeVolume::_bind_methods() {
	ClassDB::bind_method(D_METHOD("bake"), &SteamAudioProbeVolume::bake);
	ClassDB::bind_method(D_METHOD("get_num_probes"), &SteamAudioProbeVolume::get_num_probes);
	ClassDB::bind_method(D_METHOD("get_probe_positions"), &SteamAudioProbeVolume::get_probe_positions);

	ClassDB::bind_method(D_METHOD("get_size"), &SteamAudioProbeVolume::get_size);
	ClassDB::bind_method(D_METHOD("set_size", "p_size"), &SteamAudioProbeVolume::set_size);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "size"), "set_size", "get_size");

	ClassDB::bind_method(D_METHOD("get_probe_data_path"), &SteamAudioProbeVolume::get_probe_data_path);
	ClassDB::bind_method(D_METHOD("set_probe_data_path", "p_probe_data_path"), &SteamAudioProbeVolume::set_probe_data_path);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "probe_data_path", PROPERTY_HINT_FILE, "*.iplprobes"), "set_probe_data_path", "get_probe_data_path");

	ADD_GROUP("Probe Generation", "");
	ClassDB::bind_method(D_METHOD("get_generation_type"), &SteamAudioProbeVolume::get_generation_type);
	ClassDB::bind_method(D_METHOD("set_generation_type", "p_generation_type"), &SteamAudioProbeVolume::set_generation_type);
	// Order matches IPLProbeGenerationType's own values (CENTROID=0, UNIFORMFLOOR=1) - see the
	// generation_type field comment in probe_volume.hpp.
	ADD_PROPERTY(PropertyInfo(Variant::INT, "generation_type", PROPERTY_HINT_ENUM, "Centroid,Uniform Floor"), "set_generation_type", "get_generation_type");
	ClassDB::bind_method(D_METHOD("get_probe_spacing"), &SteamAudioProbeVolume::get_probe_spacing);
	ClassDB::bind_method(D_METHOD("set_probe_spacing", "p_probe_spacing"), &SteamAudioProbeVolume::set_probe_spacing);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "probe_spacing", PROPERTY_HINT_RANGE, "0.5,50.0,0.1"), "set_probe_spacing", "get_probe_spacing");
	ClassDB::bind_method(D_METHOD("get_probe_height"), &SteamAudioProbeVolume::get_probe_height);
	ClassDB::bind_method(D_METHOD("set_probe_height", "p_probe_height"), &SteamAudioProbeVolume::set_probe_height);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "probe_height", PROPERTY_HINT_RANGE, "0.1,10.0,0.1"), "set_probe_height", "get_probe_height");

	ADD_GROUP("Baking", "");
	ClassDB::bind_method(D_METHOD("get_num_rays"), &SteamAudioProbeVolume::get_num_rays);
	ClassDB::bind_method(D_METHOD("set_num_rays", "p_num_rays"), &SteamAudioProbeVolume::set_num_rays);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "num_rays", PROPERTY_HINT_RANGE, "1,16384,1"), "set_num_rays", "get_num_rays");
	ClassDB::bind_method(D_METHOD("get_num_bounces"), &SteamAudioProbeVolume::get_num_bounces);
	ClassDB::bind_method(D_METHOD("set_num_bounces", "p_num_bounces"), &SteamAudioProbeVolume::set_num_bounces);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "num_bounces", PROPERTY_HINT_RANGE, "1,64,1"), "set_num_bounces", "get_num_bounces");
	ClassDB::bind_method(D_METHOD("get_simulated_duration"), &SteamAudioProbeVolume::get_simulated_duration);
	ClassDB::bind_method(D_METHOD("set_simulated_duration", "p_simulated_duration"), &SteamAudioProbeVolume::set_simulated_duration);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "simulated_duration", PROPERTY_HINT_RANGE, "0.1,10.0,0.1"), "set_simulated_duration", "get_simulated_duration");
	ClassDB::bind_method(D_METHOD("get_saved_duration"), &SteamAudioProbeVolume::get_saved_duration);
	ClassDB::bind_method(D_METHOD("set_saved_duration", "p_saved_duration"), &SteamAudioProbeVolume::set_saved_duration);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "saved_duration", PROPERTY_HINT_RANGE, "0.1,10.0,0.1"), "set_saved_duration", "get_saved_duration");
	ClassDB::bind_method(D_METHOD("get_ambisonics_order"), &SteamAudioProbeVolume::get_ambisonics_order);
	ClassDB::bind_method(D_METHOD("set_ambisonics_order", "p_ambisonics_order"), &SteamAudioProbeVolume::set_ambisonics_order);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "ambisonics_order", PROPERTY_HINT_RANGE, "0,5,1"), "set_ambisonics_order", "get_ambisonics_order");
}

SteamAudioProbeVolume::SteamAudioProbeVolume() {
	registered.store(false);
}

SteamAudioProbeVolume::~SteamAudioProbeVolume() {
	unregister_probes();
	if (batch != nullptr) {
		iplProbeBatchRelease(&batch);
		batch = nullptr;
	}
}

void SteamAudioProbeVolume::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			ready_internal();
			break;
		case NOTIFICATION_EXIT_TREE:
			unregister_probes();
			break;
	}
}

void SteamAudioProbeVolume::ready_internal() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	String path = resolve_data_path();
	if (!FileAccess::file_exists(path)) {
		SteamAudio::log(SteamAudio::log_warn, String("SteamAudioProbeVolume: no baked data at " + path + " - call bake() first.").utf8().get_data());
		return;
	}

	Ref<FileAccess> f = FileAccess::open(path, FileAccess::READ);
	if (f.is_null()) {
		SteamAudio::log(SteamAudio::log_error, String("SteamAudioProbeVolume: failed to open " + path).utf8().get_data());
		return;
	}
	PackedByteArray bytes = f->get_buffer(f->get_length());
	f->close();

	auto gs = SteamAudioServer::get_singleton()->get_global_state();
	if (gs == nullptr) {
		return;
	}

	IPLSerializedObjectSettings ser_settings{};
	ser_settings.data = bytes.ptrw();
	ser_settings.size = bytes.size();
	IPLSerializedObject ser_obj;
	handleErr(iplSerializedObjectCreate(gs->ctx, &ser_settings, &ser_obj));

	handleErr(iplProbeBatchLoad(gs->ctx, ser_obj, &batch));
	iplSerializedObjectRelease(&ser_obj);
	iplProbeBatchCommit(batch);

	register_probes();
}

void SteamAudioProbeVolume::register_probes() {
	if (registered.load() || batch == nullptr) {
		return;
	}
	registered.store(true);
	SteamAudioServer::get_singleton()->add_probe_batch(batch);
}

void SteamAudioProbeVolume::unregister_probes() {
	if (!registered.load()) {
		return;
	}
	registered.store(false);
	SteamAudioServer::get_singleton()->remove_probe_batch(batch);
}

String SteamAudioProbeVolume::resolve_data_path() const {
	if (!probe_data_path.is_empty()) {
		return probe_data_path;
	}

	String base = get_scene_file_path();
	if (base.is_empty() && get_owner() != nullptr) {
		base = get_owner()->get_scene_file_path();
	}
	if (base.is_empty()) {
		return String("res://") + get_name() + ".iplprobes";
	}
	return base.get_basename() + "." + get_name() + ".iplprobes";
}

bool SteamAudioProbeVolume::bake() {
	auto gs = SteamAudioServer::get_singleton()->get_global_state();
	if (gs == nullptr) {
		SteamAudio::log(SteamAudio::log_error, "SteamAudioProbeVolume: cannot bake, SteamAudio is not initialized yet.");
		return false;
	}

	unregister_probes();
	if (batch != nullptr) {
		iplProbeBatchRelease(&batch);
		batch = nullptr;
	}

	// Unit-cube-to-volume transform (see IPLProbeGenerationParams): IPLMatrix4x4 is row-major
	// and applied as result = M * (u, v, w, 1), so each ROW must hold one output axis's
	// contribution from all three local axes (right/up/fwd), not one whole local axis per row.
	//
	// The phonon.h doc comment ("unit cube with min/max at (0,0,0)/(1,1,1)") is misleading -
	// the actual generator (core/src/core/probe_generator.cpp, generateUniformFloorProbes)
	// samples xPos/zPos over roughly [-0.5, +0.5], i.e. a cube CENTERED on the transform's
	// origin, not cornered at it. Passing the box's min corner as the translation (as if the
	// doc's [0,1] cube were accurate) shifts the whole sampled region down by half the box
	// size, silently truncating probe generation to only the near half of the declared volume -
	// confirmed by visual/numeric verification in Q_Mechanics. The translation must be the
	// box's center (this node's own global origin), with the column magnitudes left as the
	// full per-axis size - the generator's own xPos/zPos range already spans the full box.
	auto trf = get_global_transform();
	Vector3 right = trf.get_basis().get_column(0);
	Vector3 up = trf.get_basis().get_column(1);
	Vector3 fwd = -trf.get_basis().get_column(2);

	IPLProbeArray probe_array;
	handleErr(iplProbeArrayCreate(gs->ctx, &probe_array));

	IPLProbeGenerationParams gen_params{};
	gen_params.type = static_cast<IPLProbeGenerationType>(generation_type);
	gen_params.spacing = probe_spacing;
	gen_params.height = probe_height;
	gen_params.transform = IPLMatrix4x4{ {
			{ right.x * size.x, up.x * size.y, fwd.x * size.z, trf.origin.x },
			{ right.y * size.x, up.y * size.y, fwd.y * size.z, trf.origin.y },
			{ right.z * size.x, up.z * size.y, fwd.z * size.z, trf.origin.z },
			{ 0.f, 0.f, 0.f, 1.f },
	} };
	iplProbeArrayGenerateProbes(probe_array, gs->scene, &gen_params);

	int generated = iplProbeArrayGetNumProbes(probe_array);
	if (generated == 0) {
		SteamAudio::log(SteamAudio::log_warn, "SteamAudioProbeVolume: generated zero probes - check size/spacing/height, and that there's floor geometry already registered inside the volume.");
	}

	probe_positions.clear();
	probe_positions.resize(generated);
	for (int i = 0; i < generated; i++) {
		IPLSphere probe = iplProbeArrayGetProbe(probe_array, i);
		probe_positions[i] = Vector3(probe.center.x, probe.center.y, probe.center.z);
	}

	handleErr(iplProbeBatchCreate(gs->ctx, &batch));
	iplProbeBatchAddProbeArray(batch, probe_array);
	iplProbeBatchCommit(batch);
	iplProbeArrayRelease(&probe_array);

	IPLReflectionsBakeParams bake_params{};
	bake_params.scene = gs->scene;
	bake_params.probeBatch = batch;
	bake_params.sceneType = SteamAudioConfig::scene_type;
	bake_params.identifier = reverb_baked_data_identifier();
	bake_params.bakeFlags = IPL_REFLECTIONSBAKEFLAGS_BAKECONVOLUTION;
	bake_params.numRays = num_rays;
	bake_params.numDiffuseSamples = SteamAudioConfig::num_diffuse_samples;
	bake_params.numBounces = num_bounces;
	bake_params.simulatedDuration = simulated_duration;
	bake_params.savedDuration = saved_duration;
	bake_params.order = ambisonics_order;
	bake_params.numThreads = SteamAudioConfig::num_refl_threads;
	bake_params.irradianceMinDistance = 1.0f;
	bake_params.bakeBatchSize = 1;

	SteamAudio::log(SteamAudio::log_info, "SteamAudioProbeVolume: baking reflections (blocks until done)...");
	iplReflectionsBakerBake(gs->ctx, &bake_params, nullptr, nullptr);
	SteamAudio::log(SteamAudio::log_info, "SteamAudioProbeVolume: bake complete.");

	IPLSerializedObjectSettings ser_settings{};
	IPLSerializedObject ser_obj;
	handleErr(iplSerializedObjectCreate(gs->ctx, &ser_settings, &ser_obj));
	iplProbeBatchSave(batch, ser_obj);

	IPLsize size_bytes = iplSerializedObjectGetSize(ser_obj);
	IPLbyte *data = iplSerializedObjectGetData(ser_obj);

	String path = resolve_data_path();
	Ref<FileAccess> f = FileAccess::open(path, FileAccess::WRITE);
	bool ok = f.is_valid();
	if (ok) {
		PackedByteArray bytes;
		bytes.resize(size_bytes);
		memcpy(bytes.ptrw(), data, size_bytes);
		f->store_buffer(bytes);
		f->close();
	} else {
		SteamAudio::log(SteamAudio::log_error, String("SteamAudioProbeVolume: failed to write baked data to " + path).utf8().get_data());
	}

	iplSerializedObjectRelease(&ser_obj);

	register_probes();
	// Nothing else invalidates the gizmo when probe_positions changes - without this, the
	// SteamAudioProbeVolumeGizmoPlugin box shows up immediately (its own property, redrawn
	// on selection) but the per-probe markers stay invisible until something else happens to
	// trigger a redraw (reselecting the node, editing another property, etc.).
	update_gizmos();
	return ok;
}

int SteamAudioProbeVolume::get_num_probes() const {
	if (batch == nullptr) {
		return 0;
	}
	return iplProbeBatchGetNumProbes(batch);
}

PackedStringArray SteamAudioProbeVolume::_get_configuration_warnings() const {
	PackedStringArray res;
	if (size.x <= 0.f || size.y <= 0.f || size.z <= 0.f) {
		res.push_back("SteamAudioProbeVolume's size must be positive on all axes.");
	}
	return res;
}
