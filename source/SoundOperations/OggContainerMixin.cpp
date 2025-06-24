#include <limits.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "OggContainerMixin.hpp"

void OggContainerMixin::ParseComments(
		const OggCommentsView& comments, int total_samples, long rate, int frame_size)
{
	double temp_start = -1, temp_length = -1, temp_end = -1;

	for (int i = 0; i < comments.comments; ++i)
	{
		char *s, *comment = comments.user_comments[i];
		double *target;

		if (strncasecmp(comment, "LOOP", 4) == 0)
		{
			comment += 4;

			if (*comment == '_') {
				++comment;
			}

			if (strncasecmp(comment, "START=", 6) == 0)
			{
				target = &temp_start;
				comment += 6;
			}
			else if (strncasecmp(comment, "LENGTH=", 7) == 0)
			{
				target = &temp_length;
				comment += 7;
			}
			else if (strncasecmp(comment, "END=", 4) == 0)
			{
				target = &temp_end;
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
			else if (*s == '.')
			{
				// Interpret decimals as seconds instead of samples and convert
				for (char *frac = ++s; isdigit(*s) && s - frac < 16; ++s);

				if (!*s)
					*target = atof(comment) * rate;
			}
		}
	}

	if (temp_length > 0)
	{
		if (temp_end < 0 && temp_start >= 0)
		{
			temp_end = temp_start + temp_length;
		}
		else if (temp_start < 0 && temp_end > 0)
		{
			temp_start = temp_end - temp_length;
		}
	}
	else if (temp_end < 0 || temp_end > total_samples)
	{
		temp_end = total_samples;
	}

	loop_start = temp_start > INT_MAX ? -1 : (int) (temp_start + 0.5);
	loop_end = temp_end > INT_MAX ? -1 : (int) (temp_end + 0.5);

	if (loop_start >= 0 && loop_start < loop_end)
	{
		loop_start *= frame_size;
		loop_end *= frame_size;
	}
	else
	{
		loop_start = loop_end = -1;
	}
}

