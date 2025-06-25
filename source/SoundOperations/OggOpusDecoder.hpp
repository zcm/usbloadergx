#pragma once

#include <opusfile.h>
#include <ogc/mutex.h>

#include "SoundDecoder.hpp"
#include "OggContainerMixin.hpp"

class OggOpusDecoder : public SoundDecoder, protected OggContainerMixin
{
	public:
		OggOpusDecoder(const char * filepath);
		OggOpusDecoder(const u8 *snd, int len);
		virtual ~OggOpusDecoder();
		void Init();
		int Rewind();
		int RestartLoop();
		int Read(u8 *buffer, int buffer_size, int pos);
	protected:
		void OpenFile();
		void ParseOpusComments();
		int UnsafeRestartLoop();
		int SeekWithPreroll(ogg_int64_t pcm_offset);
		OggOpusFile *opus_file;
		mutex_t opus_mutex;
};
