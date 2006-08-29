#include "inspircd.h"
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "modes/umode_n.h"

ModeUserServerNoticeMask::ModeUserServerNoticeMask(InspIRCd* Instance) : ModeHandler(Instance, 'n', 1, 0, false, MODETYPE_USER, true)
{
}

ModeAction ModeUserServerNoticeMask::OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
{
	/* Only opers can change other users modes */
	if ((source != dest) && (!*source->oper))
		return MODEACTION_DENY;

	/* Set the bitfields */
	if (adding)
	{
		parameter = dest->ProcessNoticeMasks(parameter.c_str());
		return MODEACTION_ALLOW;
	}
	else if (dest->modes[UM_SNOMASK] != false)
	{
		dest->modes[UM_SNOMASK] = false;
		return MODEACTION_ALLOW;
	}

	/* Allow the change */
	return MODEACTION_DENY;
}

