/***************************************************************************
 * Copyright (C) 2010
 * by Dimok
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 *
 * for WiiXplorer 2010
 ***************************************************************************/
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "OggDecoder.hpp"

extern "C"  int ogg_read(void * punt, int bytes, int blocks, int *f)
{
	return ((CFile *) f)->read((u8 *) punt, bytes*blocks);
}

extern "C" int ogg_seek(int *f, ogg_int64_t offset, int mode)
{
	return ((CFile *) f)->seek((u64) offset, mode);
}

extern "C" int ogg_close(int *f)
{
	((CFile *) f)->close();
	return 0;
}

extern "C" long ogg_tell(int *f)
{
	return (long) ((CFile *) f)->tell();
}

static ov_callbacks callbacks = {
	(size_t (*)(void *, size_t, size_t, void *))  ogg_read,
	(int (*)(void *, ogg_int64_t, int))		   ogg_seek,
	(int (*)(void *))							 ogg_close,
	(long (*)(void *))							ogg_tell
};

OggDecoder::OggDecoder(const char * filepath)
	: SoundDecoder(filepath)
{
	SoundType = SOUND_OGG;

	if(!file_fd)
		return;

	OpenFile();
}

OggDecoder::OggDecoder(const u8 * snd, int len)
	: SoundDecoder(snd, len)
{
	SoundType = SOUND_OGG;

	if(!file_fd)
		return;

	OpenFile();
}

OggDecoder::~OggDecoder()
{
	ExitRequested = true;
	while(Decoding)
		usleep(100);

	if(file_fd)
		ov_clear(&ogg_file);
}

void OggDecoder::OpenFile()
{
	if (ov_open_callbacks(file_fd, &ogg_file, NULL, 0, callbacks) < 0)
	{
		delete file_fd;
		file_fd = NULL;
		return;
	}

	ogg_info = ov_info(&ogg_file, -1);

	loop_start = loop_end = -1;

	ParseComments();
	Decode();
}

void OggDecoder::ParseComments()
{
	vorbis_comment *ogg_comment = ov_comment(&ogg_file, -1);

	int loop_length = -1;

	for (int i = 0; i < ogg_comment->comments; ++i)
	{
		char *s, *comment = ogg_comment->user_comments[i];
		int *target;

		if (strncmp(comment, "LOOP", 4) == 0)
		{
			comment += 4;

			if (strncmp(comment, "START=", 6) == 0)
			{
				target = &loop_start;
				comment += 6;
			}
			else if (strncmp(comment, "LENGTH=", 7) == 0)
			{
				target = &loop_length;
				comment += 7;
			}
			else if (strncmp(comment, "END=", 4) == 0)
			{
				target = &loop_end;
				comment += 4;
			}
			else
			{
				continue;
			}

			for (s = comment; isdigit(*s) && s - comment < 10; ++s);

			if (!*s)
			{
				*target = atoi(comment);
			}
		}
	}

	if (loop_length > 0)
	{
		if (loop_end < 0 && loop_start >= 0)
		{
			loop_end = loop_start + loop_length;
		}
		else if (loop_start < 0 && loop_end > 0)
		{
			loop_start = loop_end - loop_length;
		}
	}

	if (loop_start >= 0 && loop_start < loop_end
			&& loop_start < ov_pcm_total(&ogg_file, -1))
	{
		int frame_size = GetFrameSize();

		loop_start *= frame_size;
		loop_end *= frame_size;
	}
	else
	{
		loop_start = loop_end = -1;
	}
}

int OggDecoder::GetFormat()
{
	if(!file_fd)
		return VOICE_STEREO_16BIT;

	return ((ogg_info->channels == 2) ? VOICE_STEREO_16BIT : VOICE_MONO_16BIT);
}

int OggDecoder::GetFrameSize()
{
	switch (GetFormat())
	{
		case VOICE_MONO_16BIT:
			return 2;
		case VOICE_STEREO_16BIT:
		default:
			return 4;
	}
}

int OggDecoder::GetSampleRate()
{
	if(!file_fd)
		return 0;

	return (int) ogg_info->rate;
}

int OggDecoder::Rewind()
{
	if(!file_fd)
		return -1;

	int ret = ov_time_seek(&ogg_file, 0);
	CurPos = 0;
	EndOfFile = false;

	return ret;
}

int OggDecoder::RestartLoop()
{
	if (loop_start < 0)
		return Rewind();

	CurPos = loop_start;
	EndOfFile = false;

	return ov_pcm_seek(&ogg_file, loop_start / GetFrameSize());
}

int OggDecoder::Read(u8 * buffer, int buffer_size, int pos)
{
	if(!file_fd)
		return -1;

	int bitstream = 0;

	int read = ov_read(&ogg_file, (char *) buffer, buffer_size, &bitstream);

	if(read > 0)
	{
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

	return read;
}
