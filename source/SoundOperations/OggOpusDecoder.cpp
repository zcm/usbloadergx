#include <unistd.h>
#include <malloc.h>

#include "OggOpusDecoder.hpp"

#define OPUS_STEREO_FRAME_SIZE (2 * sizeof (opus_int16))
#define OPUS_PREROLL_SAMPLES 3840  /* 80ms, not 120ms but good enough */

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

	return ParseComments(viewOf(opus_tags), -1, 48000, OPUS_STEREO_FRAME_SIZE);
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

	LWP_MutexLock(opus_mutex);
	int ret = UnsafeRestartLoop();
	LWP_MutexUnlock(opus_mutex);

	return ret;
}

int OggOpusDecoder::UnsafeRestartLoop()
{
	CurPos = loop_start;
	EndOfFile = false;

	return SeekWithPreroll(loop_start / OPUS_STEREO_FRAME_SIZE);
}

int OggOpusDecoder::SeekWithPreroll(ogg_int64_t pcm_offset)
{
	ogg_int64_t preroll = pcm_offset <= OPUS_PREROLL_SAMPLES ? 0 : pcm_offset - OPUS_PREROLL_SAMPLES;

	int ret = op_pcm_seek(opus_file, preroll);

	if (ret < 0)
		return ret;

	opus_int16 temp[1024];

	while (preroll < pcm_offset)
	{
		int read = op_read_stereo(opus_file, temp, sizeof (temp) / 2);

		if (read <= 0)
			return read;

		preroll += read;
	}

	return ret;
}

int OggOpusDecoder::Read(u8 *buffer, int buffer_size, int pos)
{
	if(!file_fd)
		return -1;

	int max_samples = buffer_size / sizeof(opus_int16);

	LWP_MutexLock(opus_mutex);

	int read = op_read_stereo(opus_file, (opus_int16 *) buffer, max_samples);

	if(read > 0)
	{
		read *= OPUS_STEREO_FRAME_SIZE;

		if (Loop && loop_start >= 0 && CurPos + read >= loop_end)
		{
			if (CurPos < loop_end)
			{
				read = loop_end - CurPos;
				UnsafeRestartLoop();
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
		read = DECODE_WITH_PARTIAL_BUFFER;
	}

	LWP_MutexUnlock(opus_mutex);

	return read;
}

#undef OPUS_STEREO_FRAME_SIZE
#undef OPUS_PREROLL_SAMPLES
