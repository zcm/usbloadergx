#pragma once

#include <tremor/ivorbisfile.h>
#include <opusfile.h>

struct OggCommentsView
{
	char **user_comments;
	int *comment_lengths;
	int comments;
	char *vendor;
};

class OggContainerMixin
{
protected:
	void ParseComments(const OggCommentsView& comments, int total_samples, long rate, int frame_size);
	int loop_start;
	int loop_end;
};

inline OggCommentsView viewOf(const vorbis_comment *vc)
{
	return { vc->user_comments, vc->comment_lengths, vc->comments, vc->vendor };
}

inline OggCommentsView viewOf(const OpusTags *ot)
{
	return { ot->user_comments, ot->comment_lengths, ot->comments, ot->vendor };
}

