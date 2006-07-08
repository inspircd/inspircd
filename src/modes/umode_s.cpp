#include "inspircd.h"
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "modes/umode_s.h"

ModeUserServerNotice::ModeUserServerNotice() : ModeHandler('s', 0, 0, false, MODETYPE_USER, false)
{
}

ModeAction ModeUserServerNotice::OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
{
	/* Only opers can change other users modes */
	if ((source != dest) && (!*source->oper))
		return MODEACTION_DENY;

	/* Set the bitfields */
	if (dest->modes[UM_SERVERNOTICE] != adding)
	{
		dest->modes[UM_SERVERNOTICE] = adding;
		return MODEACTION_ALLOW;
	}

	/* Allow the change */
	return MODEACTION_DENY;
}
