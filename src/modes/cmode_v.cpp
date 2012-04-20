/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"
#include "configreader.h"
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "modules.h"
#include "modes/cmode_v.h"

ModeChannelVoice::ModeChannelVoice(InspIRCd* Instance) : ModeHandler(Instance, 'v', 1, 1, true, MODETYPE_CHANNEL, false, '+', '%', TR_NICK)
{
}

unsigned int ModeChannelVoice::GetPrefixRank()
{
	return VOICE_VALUE;
}

ModePair ModeChannelVoice::ModeSet(User*, User*, Channel* channel, const std::string &parameter)
{
	User* x = ServerInstance->FindNick(parameter);
	if (x)
	{
		if (channel->GetStatusFlags(x) & UCMODE_VOICE)
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

void ModeChannelVoice::RemoveMode(Channel* channel, irc::modestacker* stack)
{
	CUList* clist = channel->GetVoicedUsers();
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
			std::vector<std::string> parameters; parameters.push_back(channel->name); parameters.push_back("-v"); parameters.push_back(i->first->nick);
			ServerInstance->SendMode(parameters, ServerInstance->FakeClient);
		}
	}
}

void ModeChannelVoice::RemoveMode(User*, irc::modestacker* stack)
{
}

ModeAction ModeChannelVoice::OnModeChange(User* source, User*, Channel* channel, std::string &parameter, bool adding, bool servermode)
{
	int status = channel->GetStatus(source);

	/* Call the correct method depending on wether we're adding or removing the mode */
	if (adding)
	{
		parameter = this->AddVoice(source, parameter.c_str(), channel, status);
	}
	else
	{
		parameter = this->DelVoice(source, parameter.c_str(), channel, status);
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

std::string ModeChannelVoice::AddVoice(User *user,const char* dest,Channel *chan,int status)
{
	User *d = ServerInstance->Modes->SanityChecks(user,dest,chan,status);

	if (d)
	{
		if (IS_LOCAL(user))
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_VOICE));

			if (MOD_RESULT == ACR_DENY)
				return "";
			if (MOD_RESULT == ACR_DEFAULT)
			{
				if ((status < STATUS_HOP) && (!ServerInstance->ULine(user->server)))
				{
					user->WriteServ("482 %s %s :You're not a channel (half)operator",user->nick.c_str(), chan->name.c_str());
					return "";
				}
			}
		}

		return ServerInstance->Modes->Grant(d,chan,UCMODE_VOICE);
	}
	return "";
}

std::string ModeChannelVoice::DelVoice(User *user,const char *dest,Channel *chan,int status)
{
	User *d = ServerInstance->Modes->SanityChecks(user,dest,chan,status);

	if (d)
	{
		if (IS_LOCAL(user))
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_DEVOICE));

			if (MOD_RESULT == ACR_DENY)
				return "";
			if (MOD_RESULT == ACR_DEFAULT)
			{
				if ((status < STATUS_HOP) && (!ServerInstance->ULine(user->server)))
				{
					user->WriteServ("482 %s %s :You are not a channel (half)operator",user->nick.c_str(), chan->name.c_str());
					return "";
				}
			}
		}

		return ServerInstance->Modes->Revoke(d,chan,UCMODE_VOICE);
	}
	return "";
}
