#include <string>
#include <vector>
#include "inspircd_config.h"
#include "configreader.h"
#include "hash_map.h"
#include "inspircd.h"
#include "mode.h"
#include "channels.h"
#include "users.h"

#include "commands.h"
#include "modules.h"
#include "inspstring.h"
#include "hashcomp.h"
#include "modes/cmode_o.h"

ModeChannelOp::ModeChannelOp(InspIRCd* Instance) : ModeHandler(Instance, 'o', 1, 1, true, MODETYPE_CHANNEL, false, '@')
{
}

unsigned int ModeChannelOp::GetPrefixRank()
{
	return OP_VALUE;
}

ModePair ModeChannelOp::ModeSet(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter)
{
	userrec* x = ServerInstance->FindNick(parameter);
	if (x)
	{
		if (channel->GetStatus(x) == STATUS_OP)
		{
			return std::make_pair(true, x->nick);
		}
		else
		{
			return std::make_pair(false, parameter);
		}
	}
	return std::make_pair(false, parameter);
}

ModeAction ModeChannelOp::OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
{
	int status = channel->GetStatus(source);

	/* Call the correct method depending on wether we're adding or removing the mode */
	if (adding)
	{
		parameter = this->AddOp(source, parameter.c_str(), channel, status);
	}
	else
	{
		parameter = this->DelOp(source, parameter.c_str(), channel, status);
	}
	/* If the method above 'ate' the parameter by reducing it to an empty string, then
	 * it won't matter wether we return ALLOW or DENY here, as an empty string overrides
	 * the return value and is always MODEACTION_DENY if the mode is supposed to have
	 * a parameter.
	 */
	return MODEACTION_ALLOW;
}

std::string ModeChannelOp::AddOp(userrec *user,const char* dest,chanrec *chan,int status)
{
	userrec *d = ServerInstance->Modes->SanityChecks(user,dest,chan,status);

	if (d)
	{
		if (IS_LOCAL(user))
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_OP));

			if (MOD_RESULT == ACR_DENY)
				return "";
			if (MOD_RESULT == ACR_DEFAULT)
			{
				if ((status < STATUS_OP) && (!ServerInstance->ULine(user->server)))
				{
					user->WriteServ("482 %s %s :You're not a channel operator",user->nick, chan->name);
					return "";
				}
			}
		}

		return ServerInstance->Modes->Grant(d,chan,UCMODE_OP);
	}
	return "";
}

std::string ModeChannelOp::DelOp(userrec *user,const char *dest,chanrec *chan,int status)
{
	userrec *d = ServerInstance->Modes->SanityChecks(user,dest,chan,status);

	if (d)
	{
		if (IS_LOCAL(user))
		{
			int MOD_RESULT = 0;
			ServerInstance->Log(DEBUG,"Call OnAccessCheck for AC_DEOP");
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_DEOP));

			ServerInstance->Log(DEBUG,"Returns %d",MOD_RESULT);

			if (MOD_RESULT == ACR_DENY)
				return "";
			if (MOD_RESULT == ACR_DEFAULT)
			{
				if ((status < STATUS_OP) && (!ServerInstance->ULine(user->server)) && (IS_LOCAL(user)))
				{
					user->WriteServ("482 %s %s :You are not a channel operator",user->nick, chan->name);
					return "";
				}
			}
		}

		return ServerInstance->Modes->Revoke(d,chan,UCMODE_OP);
	}
	return "";
}
