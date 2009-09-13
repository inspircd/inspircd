/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "modes/umode_o.h"

ModeUserOperator::ModeUserOperator(InspIRCd* Instance) : ModeHandler(NULL, 'o', PARAM_NONE, MODETYPE_USER)
{
	oper = true;
}

ModeAction ModeUserOperator::OnModeChange(User* source, User* dest, Channel*, std::string&, bool adding)
{
	/* Only opers can execute this class at all */
	if (!ServerInstance->ULine(source->nick.c_str()) && !ServerInstance->ULine(source->server) && source->oper.empty())
		return MODEACTION_DENY;

	/* Not even opers can GIVE the +o mode, only take it away */
	if (adding)
		return MODEACTION_DENY;

	/* Set the bitfields.
	 * Note that oper status is only given in cmd_oper.cpp
	 * NOT here. It is impossible to directly set +o without
	 * verifying as an oper and getting an opertype assigned
	 * to your User!
	 */
	char snomask = IS_LOCAL(dest) ? 'o' : 'O';
	ServerInstance->SNO->WriteToSnoMask(snomask, "User %s de-opered (by %s)", dest->nick.c_str(),
		source->nick.empty() ? source->server : source->nick.c_str());
	dest->UnOper();

	return MODEACTION_ALLOW;
}

unsigned int ModeUserOperator::GetCount()
{
	return count;
}
