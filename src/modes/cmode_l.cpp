#include "inspircd.h"
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "modes/cmode_l.h"

ModeChannelLimit::ModeChannelLimit() : ModeHandler('l', 1, 0, false, MODETYPE_CHANNEL, false)
{
}

ModeAction ModeChannelLimit::OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
{
	if (adding)
	{
		/* Setting a new limit, sanity check */
		long limit = atoi(parameter.c_str());

		/* Wrap low values at 32768 */
		if (limit < 0)
			limit = 0x7FFF;

		/* If the new limit is the same as the old limit,
		 * and the old limit isnt 0, disallow */
		if ((limit == channel->limit) && (channel->limit > 0))
		{
			parameter = "";
			return MODEACTION_DENY;
		}

		/* They must have specified an invalid number.
		 * Dont allow +l 0.
		 */
		if (!limit)
		{
			parameter = "";
			return MODEACTION_DENY;
		}

		/* Set new limit */
		channel->limit = limit;
		channel->modes[CM_LIMIT] = 1;

		return MODEACTION_ALLOW;
	}
	else
	{
		/* Check if theres a limit here to remove.
		 * If there isnt, dont allow the -l
		 */
		if (!channel->limit)
		{
			parameter = "";
			return MODEACTION_DENY;
		}

		/* Removing old limit, no checks here */
		channel->limit = 0;
		channel->modes[CM_LIMIT] = 0;

		return MODEACTION_ALLOW;
	}

	return MODEACTION_DENY;
}
