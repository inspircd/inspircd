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
#include "configreader.h"
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "modules.h"
#include "modes/cmode_h.h"

ModeChannelHalfOp::ModeChannelHalfOp(InspIRCd* Instance) : ModeHandler(Instance, NULL, 'h', 1, 1, true, MODETYPE_CHANNEL, false, '%', '@', TR_NICK)
{
}

unsigned int ModeChannelHalfOp::GetPrefixRank()
{
	return HALFOP_VALUE;
}

ModePair ModeChannelHalfOp::ModeSet(User*, User*, Channel* channel, const std::string &parameter)
{
	User* x = ServerInstance->FindNick(parameter);
	if (x)
	{
		if (channel->GetStatusFlags(x) & UCMODE_HOP)
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

void ModeChannelHalfOp::RemoveMode(Channel* channel, irc::modestacker* stack)
{
	CUList* clist = channel->GetHalfoppedUsers();
	CUList copy;

	for (CUList::iterator i = clist->begin(); i != clist->end(); i++)
	{
		User* n = i->first;
		copy.insert(std::make_pair(n,n->nick));
	}

	for (CUList::iterator i = copy.begin(); i != copy.end(); i++)
	{
		if (stack)
		{
			stack->Push(this->GetModeChar(), i->first->nick);
		}
		else
		{
			std::vector<std::string> parameters; parameters.push_back(channel->name); parameters.push_back("-h"); parameters.push_back(i->first->nick);
			ServerInstance->SendMode(parameters, ServerInstance->FakeClient);
		}
	}

}

void ModeChannelHalfOp::RemoveMode(User*, irc::modestacker* stack)
{
}

ModeAction ModeChannelHalfOp::OnModeChange(User* source, User*, Channel* channel, std::string &parameter, bool adding)
{
	int status = channel->GetStatus(source);

	/* Call the correct method depending on wether we're adding or removing the mode */
	if (adding)
	{
		parameter = this->AddHalfOp(source, parameter.c_str(), channel, status);
	}
	else
	{
		parameter = this->DelHalfOp(source, parameter.c_str(), channel, status);
	}
	/* If the method above 'ate' the parameter by reducing it to an empty string, then
	 * it won't matter wether we return ALLOW or DENY here, as an empty string overrides
	 * the return value and is always MODEACTION_DENY if the mode is supposed to have
	 * a parameter.
	 */
	if (parameter.length())
		return MODEACTION_ALLOW;
	else
		return MODEACTION_DENY;
}

std::string ModeChannelHalfOp::AddHalfOp(User *user,const char* dest,Channel *chan,int status)
{
	User *d = ServerInstance->Modes->SanityChecks(user,dest,chan,status);

	if (d)
	{
		if (IS_LOCAL(user))
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_HALFOP));

			if (MOD_RESULT == ACR_DENY)
				return "";
			if (MOD_RESULT == ACR_DEFAULT)
			{
				if ((status < STATUS_OP) && (!ServerInstance->ULine(user->server)))
				{
					user->WriteServ("482 %s %s :You're not a channel operator",user->nick.c_str(), chan->name.c_str());
					return "";
				}
			}
		}

		return ServerInstance->Modes->Grant(d,chan,UCMODE_HOP);
	}
	return "";
}

std::string ModeChannelHalfOp::DelHalfOp(User *user,const char *dest,Channel *chan,int status)
{
	User *d = ServerInstance->Modes->SanityChecks(user,dest,chan,status);

	if (d)
	{
		if (IS_LOCAL(user))
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_DEHALFOP));

			if (MOD_RESULT == ACR_DENY)
				return "";
			if (MOD_RESULT == ACR_DEFAULT)
			{
				if ((user != d) && ((status < STATUS_OP) && (!ServerInstance->ULine(user->server))))
				{
					user->WriteServ("482 %s %s :You are not a channel operator",user->nick.c_str(), chan->name.c_str());
					return "";
				}
			}
		}

		return ServerInstance->Modes->Revoke(d,chan,UCMODE_HOP);
	}
	return "";
}

