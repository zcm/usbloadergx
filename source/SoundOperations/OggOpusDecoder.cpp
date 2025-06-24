#include <unistd.h>
#include <malloc.h>

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
	Init();

	if(!file_fd)
		return;

	OpenFile();
}

OggOpusDecoder::OggOpusDecoder(const u8 *snd, int len)
	: SoundDecoder(snd, len)
{
	Init();

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

	LWP_MutexDestroy(opus_mutex);
}

void OggOpusDecoder::Init()
{
	SoundType = SOUND_OPUS;

	LWP_MutexInit(&opus_mutex, false);
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

	ParseOpusComments();
	Decode();
}

void OggOpusDecoder::ParseOpusComments()
{
	const OpusTags *opus_tags = op_tags(opus_file, -1);
	int total_samples = op_pcm_total(opus_file, -1);

	return ParseComments(viewOf(opus_tags), total_samples, 48000, 4);
}

int OggOpusDecoder::Rewind()
{
	if(!file_fd)
		return -1;

	LWP_MutexLock(opus_mutex);
	int ret = op_pcm_seek(opus_file, 0);
	LWP_MutexUnlock(opus_mutex);

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

	LWP_MutexLock(opus_mutex);
	int ret = op_pcm_seek(opus_file, loop_start);
	LWP_MutexUnlock(opus_mutex);

	return ret;
}

int OggOpusDecoder::Read(u8 *buffer, int buffer_size, int pos)
{
	if(!file_fd)
		return -1;

	int max_samples = buffer_size / sizeof(opus_int16);

	LWP_MutexLock(opus_mutex);
	int read = op_read_stereo(opus_file, (opus_int16 *) buffer, max_samples);
	LWP_MutexUnlock(opus_mutex);

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
