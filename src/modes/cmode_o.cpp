/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
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
#include "modes/cmode_o.h"

ModeChannelOp::ModeChannelOp(InspIRCd* Instance) : ModeHandler(Instance, 'o', 1, 1, true, MODETYPE_CHANNEL, false, '@', '@', TR_NICK)
{
}

unsigned int ModeChannelOp::GetPrefixRank()
{
	return OP_VALUE;
}

ModePair ModeChannelOp::ModeSet(User*, User*, Channel* channel, const std::string &parameter)
{
	User* x = ServerInstance->FindNick(parameter);
	if (x)
	{
		if (channel->GetStatusFlags(x) & UCMODE_OP)
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


void ModeChannelOp::RemoveMode(Channel* channel, irc::modestacker* stack)
{
	CUList* clist = channel->GetOppedUsers();
	CUList copy;

	for (CUList::iterator i = clist->begin(); i != clist->end(); i++)
	{
		User* n = i->first;
		copy.insert(std::make_pair(n,n->nick));
	}

	for (CUList::iterator i = copy.begin(); i != copy.end(); i++)
	{
		if (stack)
			stack->Push(this->GetModeChar(), i->first->nick);
		else
		{
			std::vector<std::string> parameters; parameters.push_back(channel->name); parameters.push_back("-o"); parameters.push_back(i->first->nick);
			ServerInstance->SendMode(parameters, ServerInstance->FakeClient);
		}
	}
}

void ModeChannelOp::RemoveMode(User*, irc::modestacker* stack)
{
}

ModeAction ModeChannelOp::OnModeChange(User* source, User*, Channel* channel, std::string &parameter, bool adding, bool servermode)
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
	if (parameter.length())
		return MODEACTION_ALLOW;
	else
		return MODEACTION_DENY;
}

std::string ModeChannelOp::AddOp(User *user,const char* dest,Channel *chan,int status)
{
	User *d = ServerInstance->Modes->SanityChecks(user,dest,chan,status);

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
					user->WriteServ("482 %s %s :You're not a channel operator",user->nick.c_str(), chan->name.c_str());
					return "";
				}
			}
		}

		return ServerInstance->Modes->Grant(d,chan,UCMODE_OP);
	}
	return "";
}

std::string ModeChannelOp::DelOp(User *user,const char *dest,Channel *chan,int status)
{
	User *d = ServerInstance->Modes->SanityChecks(user,dest,chan,status);

	if (d)
	{
		if (IS_LOCAL(user))
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_DEOP));

			if (MOD_RESULT == ACR_DENY)
				return "";
			if (MOD_RESULT == ACR_DEFAULT)
			{
				if ((status < STATUS_OP) && (!ServerInstance->ULine(user->server)) && (IS_LOCAL(user)))
				{
					user->WriteServ("482 %s %s :You are not a channel operator",user->nick.c_str(), chan->name.c_str());
					return "";
				}
			}
		}

		return ServerInstance->Modes->Revoke(d,chan,UCMODE_OP);
	}
	return "";
}
