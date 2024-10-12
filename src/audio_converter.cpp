#include "audio_converter.h"

#include <godot_cpp/classes/ogg_packet_sequence.hpp>

#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_STDIO
#define MINIMP3_IMPLEMENTATION
#include "minimp3_ex.h"

#include <vorbis/codec.h>

void AudioConverter::_bind_methods() {
	ClassDB::bind_static_method("AudioConverter", D_METHOD("convert_mp3", "mp3"), &AudioConverter::convert_mp3);
	ClassDB::bind_static_method("AudioConverter", D_METHOD("convert_vorbis", "vorbis"), &AudioConverter::convert_vorbis);
}

Ref<AudioStreamWAV> AudioConverter::convert_mp3(const Ref<AudioStreamMP3> &mp3) {
	ERR_FAIL_COND_V(mp3.is_null(), Ref<AudioStreamWAV>());

	PackedByteArray mp3_data = mp3->get_data();

	mp3dec_t dec{};
	mp3dec_file_info_t info{};

	struct BufferDeleter {
		mp3dec_file_info_t &i;
		BufferDeleter(mp3dec_file_info_t &i) : i(i) {}
		~BufferDeleter() { ::free(i.buffer); }
	} buffer_deleter(info);

	int error = mp3dec_load_buf(&dec, mp3_data.ptr(), mp3_data.size(), &info, nullptr, nullptr);
	ERR_FAIL_COND_V(error != 0, Ref<AudioStreamWAV>());

	static_assert(sizeof(*info.buffer) == 2);
	PackedByteArray pcm_data;
	pcm_data.resize(info.samples * 2);
	memcpy(pcm_data.ptrw(), info.buffer, info.samples * 2);

	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_data(pcm_data);
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(info.hz);
	wav->set_stereo(info.channels > 1);
	if (mp3->has_loop()) {
		wav->set_loop_mode(AudioStreamWAV::LOOP_FORWARD);
		wav->set_loop_begin(Math::fast_ftoi(info.hz * mp3->get_loop_offset()));
	}
	return wav;
}

Ref<AudioStreamWAV> AudioConverter::convert_vorbis(const Ref<AudioStreamOggVorbis> &vorbis) {
	ERR_FAIL_COND_V(vorbis.is_null(), Ref<AudioStreamWAV>());
	Ref<OggPacketSequence> packet_sequence = vorbis->get_packet_sequence();
	ERR_FAIL_COND_V(packet_sequence.is_null(), Ref<AudioStreamWAV>());
	TypedArray<Array> packets = packet_sequence->get_packet_data();
	PackedInt64Array granules = packet_sequence->get_packet_granule_positions();

	vorbis_info info{};
	vorbis_info_init(&info);
	vorbis_comment comment{};
	vorbis_comment_init(&comment);
	vorbis_dsp_state dsp_state{};
	bool dsp_state_is_allocated = false;
	vorbis_block block{};
	bool block_is_allocated = false;

	struct Clearer {
		Clearer(vorbis_info *i, vorbis_comment *c, vorbis_dsp_state *d, bool &da, vorbis_block *b, bool &ba) : i(i), c(c), d(d), da(da), b(b), ba(ba) {}
		~Clearer() {
			if (ba)
				vorbis_block_clear(b);
			if (da)
				vorbis_dsp_clear(d);
			vorbis_comment_clear(c);
			vorbis_info_clear(i);
		}
		vorbis_info *i;
		vorbis_comment *c;
		vorbis_dsp_state *d;
		bool &da;
		vorbis_block *b;
		bool &ba;
	} clearer(&info, &comment, &dsp_state, dsp_state_is_allocated, &block, block_is_allocated);

	int64_t packetno = 0;
	int err = 0;
	PackedByteArray pcm_data;
	for (int64_t page_cursor = 0; page_cursor < packets.size(); page_cursor++) {
		Array page = packets[page_cursor];
		for (int64_t packet_cursor = 0; packet_cursor < page.size(); packet_cursor++) {
			PackedByteArray packet_data = page[packet_cursor];

			ogg_packet packet{};
			packet.packet = packet_data.ptrw();
			packet.bytes = packet_data.size();
			packet.b_o_s = page_cursor == 0 && packet_cursor == 0;
			packet.e_o_s = page_cursor == packets.size() - 1 && packet_cursor == page.size() - 1;
			packet.granulepos = packet_cursor == page.size() - 1 ? granules[page_cursor] : -1;
			packet.packetno = packetno++;

			if (packetno <= 3) {
				if (packetno == 0) {
					err = vorbis_synthesis_idheader(&packet);
					ERR_FAIL_COND_V(err != 0, Ref<AudioStreamWAV>());
				}

				err = vorbis_synthesis_headerin(&info, &comment, &packet);
				ERR_FAIL_COND_V(err != 0, Ref<AudioStreamWAV>());

				if (packetno == 3) {
					err = vorbis_synthesis_init(&dsp_state, &info);
					ERR_FAIL_COND_V(err != 0, Ref<AudioStreamWAV>());
					dsp_state_is_allocated = true;

					err = vorbis_block_init(&dsp_state, &block);
					ERR_FAIL_COND_V(err != 0, Ref<AudioStreamWAV>());
					block_is_allocated = true;
				}
			} else {
				err = vorbis_synthesis(&block, &packet);
				ERR_FAIL_COND_V(err != 0, Ref<AudioStreamWAV>());

				err = vorbis_synthesis_blockin(&dsp_state, &block);
				ERR_FAIL_COND_V(err != 0, Ref<AudioStreamWAV>());

				float **samples = nullptr;
				int num_samples = vorbis_synthesis_pcmout(&dsp_state, &samples);

				for (int sample = 0; sample < num_samples; sample++) {
					int16_t s = Math::fast_ftoi(samples[0][sample] * 32767.0);
					pcm_data.append(uint8_t(s & 0xff));
					pcm_data.append(uint8_t(s >> 8));

					if (info.channels > 1) {
						s = Math::fast_ftoi(samples[1][sample] * 32767.0);
						pcm_data.append(uint8_t(s & 0xff));
						pcm_data.append(uint8_t(s >> 8));
					}
				}

				err = vorbis_synthesis_read(&dsp_state, num_samples);
				ERR_FAIL_COND_V(err != 0, Ref<AudioStreamWAV>());
			}
		}
	}

	Ref<AudioStreamWAV> wav;
	wav.instantiate();
	wav->set_data(pcm_data);
	wav->set_format(AudioStreamWAV::FORMAT_16_BITS);
	wav->set_mix_rate(info.rate);
	wav->set_stereo(info.channels > 1);
	if (vorbis->has_loop()) {
		wav->set_loop_mode(AudioStreamWAV::LOOP_FORWARD);
		wav->set_loop_begin(Math::fast_ftoi(info.rate * vorbis->get_loop_offset()));
	}
	return wav;
}
