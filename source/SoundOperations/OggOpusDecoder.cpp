#include <limits.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "OggOpusDecoder.hpp"

extern "C"
{

int opus_read(void *stream, unsigned char *ptr, int nbytes)
{
	return ((CFile *) stream)->read((u8 *) ptr, nbytes);
}

int opus_seek(void *stream, opus_int64 offset, int whence)
{
	return ((CFile *) stream)->seek((u64) offset, whence);
}

opus_int64 opus_tell(void *stream)
{
	return (opus_int64) ((CFile *) stream)->tell();
}

int opus_close(void *stream)
{
	((CFile *) stream)->close();
	return 0;
}

}

static OpusFileCallbacks callbacks = {
	(int (*)(void *, unsigned char *, int)) opus_read,
	(int (*)(void *, opus_int64, int))      opus_seek,
	(opus_int64 (*)(void *))                opus_tell,
	(int (*)(void *))                       opus_close,
};

OggOpusDecoder::OggOpusDecoder(const char *filepath)
	: SoundDecoder(filepath)
{
	if(!file_fd)
		return;

	OpenFile();
}

OggOpusDecoder::OggOpusDecoder(const u8 *snd, int len)
	: SoundDecoder(snd, len)
{
	if(!file_fd)
		return;

	OpenFile();
}

OggOpusDecoder::~OggOpusDecoder()
{
	ExitRequested = true;
	while(Decoding)
		usleep(100);

	if(file_fd)
		op_free(opus_file);
}

void OggOpusDecoder::Init()
{
	SoundDecoder::Init();

	SoundType = SOUND_OPUS;

	// Worst case, 120ms frame size @ 48KHz, 2 channels
	SoundBlockSize = 11520 * sizeof(opus_int16);
	SoundBuffer.SetBufferBlockSize(SoundBlockSize);
	SoundBuffer.Resize(SoundBlocks);
}

void OggOpusDecoder::OpenFile()
{
	if (!(opus_file = op_open_callbacks(file_fd, &callbacks, NULL, 0, NULL)))
	{
		delete file_fd;
		file_fd = NULL;
		return;
	}

	loop_start = loop_end = -1;

	ParseComments();
	Decode();
}

void OggOpusDecoder::ParseComments()
{
}

int OggOpusDecoder::Rewind()
{
	if(!file_fd)
		return -1;

	int ret = op_pcm_seek(opus_file, 0);
	CurPos = 0;
	EndOfFile = false;

	return ret;
}

int OggOpusDecoder::RestartLoop()
{
	if (loop_start < 0)
		return Rewind();

	CurPos = loop_start;
	EndOfFile = false;

	return op_pcm_seek(opus_file, loop_start);
}

int OggOpusDecoder::Read(u8 *buffer, int buffer_size, int pos)
{
	if(!file_fd)
		return -1;

	int max_samples = buffer_size / sizeof(opus_int16);

	int read = op_read_stereo(opus_file, (opus_int16 *) buffer, max_samples);

	if(read > 0)
	{
		read *= 2 * sizeof(opus_int16);

		if (Loop && loop_start >= 0 && CurPos + read >= loop_end)
		{
			if (CurPos < loop_end)
			{
				read = loop_end - CurPos;
				RestartLoop();
			}
			else
			{
				read = 0;  // RestartLoop() will be called for us by Decode()
			}
		}
		else
		{
			CurPos += read;
		}
	}
	else if (read == 0 && CurPos < op_pcm_total(opus_file, -1) * sizeof(opus_int16))
	{
		return DECODE_WITH_PARTIAL_BUFFER;
	}

	return read;
}
