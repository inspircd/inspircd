#include "inspircd.h"
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "modes/umode_o.h"

ModeUserOperator::ModeUserOperator() : ModeHandler('o', 0, 0, false, MODETYPE_USER, true)
{
}

ModeAction ModeUserOperator::OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
{
	/* Only opers can execute this class at all */
	if (!*source->oper)
		return MODEACTION_DENY;

	/* Not even opers can GIVE the +o mode, only take it away */
	if (adding)
		return MODEACTION_DENY;

	/* Set the bitfields.
	 * Note that oper status is only given in cmd_oper.cpp
	 * NOT here. It is impossible to directly set +o without
	 * verifying as an oper and getting an opertype assigned
	 * to your userrec!
	 */
	dest->modes[UM_OPERATOR] = 0;
	return MODEACTION_ALLOW;
}
