#include "stream.hpp"
#include "config.hpp"
#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/variant/packed_vector2_array.hpp"
#include "server.hpp"
#include "steam_audio.hpp"
#include <phonon.h>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <algorithm>
#include <cstring>

SteamAudioStream::SteamAudioStream() {}
SteamAudioStream::~SteamAudioStream() {}

void SteamAudioStream::_bind_methods() {}

Ref<AudioStreamPlayback> SteamAudioStream::_instantiate_playback() const {
	Ref<SteamAudioStreamPlayback> playback;
	playback.instantiate();
	playback->set_stream(stream);
	playback->parent = parent;
	playback->registry = registry;

	{
		std::lock_guard lock(registry->mux);
		registry->live.push_back(playback.ptr());
	}

	return playback;
}

void SteamAudioStream::set_stream(Ref<AudioStream> p_stream) { stream = p_stream; }
Ref<AudioStream> SteamAudioStream::get_stream() { return this->stream; }

void SteamAudioStream::detach_all_playbacks() const {
	std::lock_guard lock(registry->mux);
	for (SteamAudioStreamPlayback *p : registry->live) {
		std::lock_guard plock(p->parent_mux);
		p->parent = nullptr;
	}
}

// ----------------------------------------------------
// SteamAudioStreamPlayback

SteamAudioStreamPlayback::SteamAudioStreamPlayback() {}
SteamAudioStreamPlayback::~SteamAudioStreamPlayback() {
	// Must run before any member is torn down: SteamAudioStream::detach_all_playbacks() holds
	// registry->mux while it locks each live playback's parent_mux in turn, so this blocks here
	// until that finishes rather than letting parent_mux (or `this`) get destroyed out from
	// under a detach_all_playbacks() call already in progress. `registry` is a shared_ptr, so
	// this is safe even if the originating SteamAudioStream was already destroyed.
	if (registry) {
		std::lock_guard lock(registry->mux);
		auto it = std::find(registry->live.begin(), registry->live.end(), this);
		if (it != registry->live.end()) {
			registry->live.erase(it);
		}
	}
}

int32_t SteamAudioStreamPlayback::_mix(AudioFrame *buffer, float rate_scale, int32_t frames) {
	// Held for the entire function, always taken before ls->mux below - see the comment on
	// parent_mux in stream.hpp. This is what makes `parent` (and everything reached through it)
	// safe to dereference for the rest of _mix(): ~SteamAudioPlayer() cannot get past
	// detach_all_playbacks() while this lock is held, so the player can't be mid-destruction.
	std::unique_lock plock(parent_mux);
	if (parent == nullptr) {
		return frames;
	}

	if (stream_playback.is_null()) {
		return frames;
	}

	if (Engine::get_singleton()->is_editor_hint()) {
		return frames;
	}

	auto gs = SteamAudioServer::get_singleton()->get_global_state(false);
	if (gs == nullptr) {
		return frames;
	}

	SteamAudio::log(SteamAudio::log_debug, "mixing");

	LocalSteamAudioState *ls = parent->get_local_state();
	if (ls == nullptr) { // probably being destroyed
		return frames;
	}
	std::unique_lock lock(ls->mux);

	ls = parent->get_local_state();
	if (ls == nullptr || !ls->src.player) {
		return frames;
	}

	PackedVector2Array mixed_frames = stream_playback->mix_audio(rate_scale, frames);
	frames = int(mixed_frames.size());

	auto gs_local = SteamAudioServer::get_singleton()->get_global_state(false);
	if (gs_local != nullptr && frames > gs_local->audio_cfg.frameSize) {
		frames = gs_local->audio_cfg.frameSize;
	}

	for (int i = 0; i < frames; i++) {
		ls->bufs.in.data[0][i] = mixed_frames[i].x;
		ls->bufs.in.data[1][i] = mixed_frames[i].y;
	}

	if (ls->cfg.is_air_absorp_on) {
		ls->direct_outputs.flags = static_cast<IPLDirectEffectFlags>(
				ls->direct_outputs.flags |
				IPL_DIRECTEFFECTFLAGS_APPLYAIRABSORPTION);
	}

	if (ls->cfg.is_dist_attn_on) {
		ls->direct_outputs.flags = static_cast<IPLDirectEffectFlags>(
				ls->direct_outputs.flags |
				IPL_DIRECTEFFECTFLAGS_APPLYDISTANCEATTENUATION);
	}
	if (ls->cfg.is_occlusion_on) {
		ls->direct_outputs.flags = static_cast<IPLDirectEffectFlags>(
				ls->direct_outputs.flags |
				IPL_DIRECTEFFECTFLAGS_APPLYOCCLUSION |
				IPL_DIRECTEFFECTFLAGS_APPLYTRANSMISSION);
		ls->direct_outputs.transmissionType = ls->cfg.transmission_type;
	}
	if (ls->cfg.is_directivity_on) {
		ls->direct_outputs.flags = static_cast<IPLDirectEffectFlags>(
				ls->direct_outputs.flags |
				IPL_DIRECTEFFECTFLAGS_APPLYDIRECTIVITY);
	}

	if (ls->direct_outputs.flags != 0) {
		iplDirectEffectApply(
				ls->fx.direct, &ls->direct_outputs,
				&ls->bufs.in, &ls->bufs.direct);
	} else {
		for (int i = 0; i < ls->bufs.direct.numChannels; i++) {
			for (int j = 0; j < ls->bufs.direct.numSamples; j++) {
				ls->bufs.direct.data[i][j] = 0.0f;
			}
		}

		iplAudioBufferMix(gs->ctx, &ls->bufs.in, &ls->bufs.direct);
	}

	IPLAmbisonicsDecodeEffectParams dec_params{};
	dec_params.orientation = gs->listener_coords;
	dec_params.order = ls->cfg.ambisonics_order;
	dec_params.hrtf = gs->hrtf;
	dec_params.binaural = IPL_TRUE;

	if (ls->cfg.is_ambisonics_on) {
		IPLAmbisonicsEncodeEffectParams enc_params{};
		enc_params.direction = ipl_vec3_from(ls->dir_to_listener);
		enc_params.order = ls->cfg.ambisonics_order;
		iplAmbisonicsEncodeEffectApply(
				ls->fx.enc, &enc_params,
				&ls->bufs.direct, &ls->bufs.ambi);

		iplAmbisonicsDecodeEffectApply(
				ls->fx.dec, &dec_params,
				&ls->bufs.ambi, &ls->bufs.out);
		SteamAudio::log(SteamAudio::log_debug, "mixing: finished ambisonics");
	} else {
		// iplAudioBufferMix adds into the destination instead of replacing it; clear first or it
		// accumulates every callback into runaway buzzing (upstream #128).
		for (int i = 0; i < ls->bufs.out.numChannels; i++) {
			memset(ls->bufs.out.data[i], 0, ls->bufs.out.numSamples * sizeof(float));
		}
		iplAudioBufferMix(gs->ctx, &ls->bufs.direct, &ls->bufs.out);
	}

	gs->refl_ir_lock.lock();
	if (ls->refl_outputs.ir != nullptr && ls->cfg.is_reflection_on) {
		iplAudioBufferDownmix(gs->ctx, &ls->bufs.in, &ls->bufs.mono);
		ls->refl_outputs.numChannels = ambisonic_channels_from(ls->cfg.ambisonics_order);
		ls->refl_outputs.type = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION;
		ls->refl_outputs.irSize = int(SteamAudioConfig::max_refl_duration * float(gs->audio_cfg.samplingRate));
		iplReflectionEffectApply(ls->fx.refl, &ls->refl_outputs, &ls->bufs.mono, &ls->bufs.refl_ambi, nullptr);

		iplAmbisonicsDecodeEffectApply(
				ls->fx.refl_dec, &dec_params,
				&ls->bufs.refl_ambi, &ls->bufs.refl_out);

		SteamAudio::log(SteamAudio::log_debug, "mixing: mixing reflection and direct buffers");
		iplAudioBufferMix(gs->ctx, &ls->bufs.refl_out, &ls->bufs.out);
	}
	gs->refl_ir_lock.unlock();

	for (int i = 0; i < frames; i++) {
		buffer[i].left = ls->bufs.out.data[0][i];
		buffer[i].right = ls->bufs.out.data[1][i];
	}

	SteamAudio::log(SteamAudio::log_debug, "mixing: done");
	return frames;
}

void SteamAudioStreamPlayback::_bind_methods() {
	ClassDB::bind_method(D_METHOD("play_stream", "stream", "from_offset", "volume_db", "pitch_scale"), &SteamAudioStreamPlayback::play_stream, DEFVAL(0), DEFVAL(0), DEFVAL(1.0));
}

int SteamAudioStreamPlayback::play_stream(const Ref<AudioStream> &p_stream, float p_from_offset, float p_volume_db, float p_pitch_scale) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return 0;
	}

	stream = p_stream;
	stream_playback = stream->instantiate_playback();
	stream_playback->start(p_from_offset);

	return 0;
}

void SteamAudioStreamPlayback::_start(double from_pos) {
	if (stream_playback == nullptr) {
		if (stream != nullptr) {
			is_active.store(true);
			play_stream(stream, float(from_pos), 0.0, 1.0); // FIXME: do not assume these params
		}
		return;
	} else if (stream_playback->is_playing()) {
		return;
	}
	stream_playback->start(from_pos);
	is_active.store(true);
}

void SteamAudioStreamPlayback::_stop() {
	is_active.store(false);
	if (stream_playback == nullptr || !stream_playback->is_playing()) {
		return;
	}
	stream_playback->stop();
}

bool SteamAudioStreamPlayback::_is_playing() const { return is_active; }
void SteamAudioStreamPlayback::set_stream(Ref<AudioStream> p_stream) { stream = p_stream; }
Ref<AudioStreamPlayback> SteamAudioStreamPlayback::get_stream_playback() { return this->stream_playback; }
