#ifndef STEAM_AUDIO_STREAM_H
#define STEAM_AUDIO_STREAM_H

#include "godot_cpp/classes/ref.hpp"
#include "player.hpp"
#include <phonon.h>
#include <godot_cpp/classes/audio_frame.hpp>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_playback.hpp>
#include <godot_cpp/templates/safe_refcount.hpp>
#include <memory>
#include <mutex>
#include <vector>

using namespace godot;

class SteamAudio;
class SteamAudioStreamPlayback;

// Registry of every playback a SteamAudioStream has ever instantiated, still alive - lets
// ~SteamAudioPlayer() null every playback's `parent`, not just the one cached in
// SteamAudioPlayer::pb (see SteamAudioStream::detach_all_playbacks()). Godot's AudioServer can
// keep a playback alive well past its originating SteamAudioStream's own lifetime (that
// decoupling is the whole reason `pb` existed in the first place), so this can't live as a plain
// member of either side - it's heap-allocated and shared, kept alive by whichever of the stream
// or its playbacks outlives the other.
struct SteamAudioPlaybackRegistry {
	std::mutex mux;
	std::vector<SteamAudioStreamPlayback *> live;
};

class SteamAudioStream : public AudioStream {
	GDCLASS(SteamAudioStream, AudioStream)
	friend class SteamAudioStreamPlayback;
	Ref<AudioStream> stream;

	std::shared_ptr<SteamAudioPlaybackRegistry> registry = std::make_shared<SteamAudioPlaybackRegistry>();

protected:
	static void _bind_methods();

public:
	SteamAudioStream();
	~SteamAudioStream();

	Ref<AudioStreamPlayback> _instantiate_playback() const override;
	void set_stream(Ref<AudioStream> p_stream);
	Ref<AudioStream> get_stream();

	// Nulls `parent` on every still-registered playback, each under that playback's own
	// parent_mux. Called from the very start of ~SteamAudioPlayer(), before anything else -
	// blocks until any _mix() call already in flight (which holds parent_mux for its whole
	// duration) finishes, so by the time this returns no thread can still be mid-dereference
	// of the player being destroyed.
	void detach_all_playbacks() const;

	SteamAudioPlayer *parent = nullptr;
};

class SteamAudioStreamPlayback : public AudioStreamPlayback {
	GDCLASS(SteamAudioStreamPlayback, AudioStreamPlayback);
	friend class SteamAudioStream;

private:
	Ref<AudioStream> stream;
	Ref<AudioStreamPlayback> stream_playback;

	std::atomic<bool> is_active{false};

	// Shared with the originating SteamAudioStream (and every sibling playback it created) - see
	// SteamAudioPlaybackRegistry. Set in SteamAudioStream::_instantiate_playback(), which also
	// registers `this` into it; the destructor unregisters.
	std::shared_ptr<SteamAudioPlaybackRegistry> registry;

protected:
	static void _bind_methods();

public:
	SteamAudioStreamPlayback();
	~SteamAudioStreamPlayback();

	void set_stream(Ref<AudioStream> p_stream);
	Ref<AudioStreamPlayback> get_stream_playback();

	virtual int32_t _mix(AudioFrame *buffer, float rate_scale, int32_t frames) override;
	int play_stream(const Ref<AudioStream> &p_stream, float p_from_offset,
			float p_volume_db, float p_pitch_scale);
	void _start(double from_pos) override;
	void _stop() override;
	bool _is_playing() const override;

	// Guards `parent` against SteamAudioPlayer's destructor. _mix() holds this for its entire
	// body (parent_mux before ls->mux, always - see stream.cpp); detach_all_playbacks() only
	// ever holds it briefly to null `parent`, never nested with local_state.mux. That strict
	// ordering is what makes this deadlock-safe against ~SteamAudioPlayer()'s own locking.
	std::mutex parent_mux;
	SteamAudioPlayer *parent = nullptr;
};

#endif
