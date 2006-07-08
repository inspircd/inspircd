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
		return MODEACTION_ALLOW;

	/* Set the bitfields */
	adding ? dest->modebits |= UM_WALLOPS : dest->modebits &= ~UM_WALLOPS;

	/* Use the bitfields to build the user's mode string */
	ModeParser::BuildModeString(dest);

	/* Allow the change */
	return MODEACTION_ALLOW;
}
