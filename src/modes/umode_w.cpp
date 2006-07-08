#include "inspircd.h"
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "modes/umode_w.h"

ModeUserWallops::ModeUserWallops() : ModeHandler('w', 0, 0, false, MODETYPE_USER, false)
{
}

ModeAction ModeUserWallops::OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
{
	/* Only opers can change other users modes */
	if ((source != dest) && (!*source->oper))
		return MODEACTION_DENY;

	/* Set the bitfields */
	if (dest->modes[UM_WALLOPS] != adding)
	{
		dest->modes[UM_WALLOPS] = adding;
		return MODEACTION_ALLOW;
	}

	/* Allow the change */
	return MODEACTION_DENY;
}
