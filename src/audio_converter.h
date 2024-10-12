#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>

#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/audio_stream_mp3.hpp>
#include <godot_cpp/classes/audio_stream_ogg_vorbis.hpp>

using namespace godot;

class AudioConverter : public Object {
	GDCLASS(AudioConverter, Object);

protected:
	static void _bind_methods();

public:
	static Ref<AudioStreamWAV> convert_mp3(const Ref<AudioStreamMP3> &mp3);
	static Ref<AudioStreamWAV> convert_vorbis(const Ref<AudioStreamOggVorbis> &vorbis);
};
