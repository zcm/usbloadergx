#include <opusfile.h>

#include "SoundDecoder.hpp"

class OggOpusDecoder : public SoundDecoder
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
		void ParseComments();
		OggOpusFile *opus_file;
		int loop_start;
		int loop_end;
};
