/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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
#include <string>
#include <vector>
#include "inspircd_config.h"
#include "configreader.h"
#include "hash_map.h"
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "modules.h"
#include "inspstring.h"
#include "hashcomp.h"
#include "modes/cmode_b.h"

ModeChannelBan::ModeChannelBan(InspIRCd* Instance) : ModeHandler(Instance, 'b', 1, 1, true, MODETYPE_CHANNEL, false)
{
}

ModeAction ModeChannelBan::OnModeChange(User* source, User*, Channel* channel, std::string &parameter, bool adding, bool servermode)
{
	int status = channel->GetStatus(source);
	/* Call the correct method depending on wether we're adding or removing the mode */
	if (adding)
	{
		parameter = this->AddBan(source, parameter, channel, status, servermode);
	}
	else
	{
		parameter = this->DelBan(source, parameter, channel, status);
	}
	/* If the method above 'ate' the parameter by reducing it to an empty string, then
	 * it won't matter wether we return ALLOW or DENY here, as an empty string overrides
	 * the return value and is always MODEACTION_DENY if the mode is supposed to have
	 * a parameter.
	 */
	return MODEACTION_ALLOW;
}

void ModeChannelBan::RemoveMode(Channel* channel, irc::modestacker* stack)
{
	BanList copy;

	for (BanList::iterator i = channel->bans.begin(); i != channel->bans.end(); i++)
	{
		copy.push_back(*i);
	}

	for (BanList::iterator i = copy.begin(); i != copy.end(); i++)
	{
		if (stack)
		{
			stack->Push(this->GetModeChar(), i->data);
		}
		else
		{
			std::vector<std::string> parameters; parameters.push_back(channel->name); parameters.push_back("-b"); parameters.push_back(i->data);
			ServerInstance->SendMode(parameters, ServerInstance->FakeClient);
		}
	}
}

void ModeChannelBan::RemoveMode(User*, irc::modestacker* stack)
{
}

void ModeChannelBan::DisplayList(User* user, Channel* channel)
{
	/* Display the channel banlist */
	for (BanList::reverse_iterator i = channel->bans.rbegin(); i != channel->bans.rend(); ++i)
	{
		user->WriteServ("367 %s %s %s %s %lu",user->nick.c_str(), channel->name.c_str(), i->data.c_str(), i->set_by.c_str(), (unsigned long)i->set_time);
	}
	user->WriteServ("368 %s %s :End of channel ban list",user->nick.c_str(), channel->name.c_str());
	return;
}

void ModeChannelBan::DisplayEmptyList(User* user, Channel* channel)
{
	user->WriteServ("368 %s %s :End of channel ban list",user->nick.c_str(), channel->name.c_str());
}

std::string& ModeChannelBan::AddBan(User *user, std::string &dest, Channel *chan, int, bool servermode)
{
	if ((!user) || (!chan))
	{
		ServerInstance->Logs->Log("MODE",DEFAULT,"*** BUG *** AddBan was given an invalid parameter");
		dest = "";
		return dest;
	}

	/* Attempt to tidy the mask */
	ModeParser::CleanMask(dest);
	/* If the mask was invalid, we exit */
	if (dest == "")
		return dest;

	long maxbans = chan->GetMaxBans();
	if (IS_LOCAL(user) && ((unsigned)chan->bans.size() > (unsigned)maxbans))
	{
		user->WriteServ("478 %s %s :Channel ban list for %s is full (maximum entries for this channel is %ld)",user->nick.c_str(), chan->name.c_str(), chan->name.c_str(), maxbans);
		dest = "";
		return dest;
	}

	int MOD_RESULT = 0;
	FOREACH_RESULT(I_OnAddBan,OnAddBan(user,chan,dest));
	if (MOD_RESULT)
	{
		dest = "";
		return dest;
	}

	for (BanList::iterator i = chan->bans.begin(); i != chan->bans.end(); i++)
	{
		if (i->data == dest)
		{
			/* dont allow a user to set the same ban twice */
			dest = "";
			return dest;
		}
	}

	b.set_time = ServerInstance->Time();
	b.data.assign(dest, 0, MAXBUF);
	b.set_by.assign(servermode ? ServerInstance->Config->ServerName : user->nick, 0, 64);
	chan->bans.push_back(b);
	return dest;
}

ModePair ModeChannelBan::ModeSet(User*, User*, Channel* channel, const std::string &parameter)
{
	for (BanList::iterator i = channel->bans.begin(); i != channel->bans.end(); i++)
	{
		if (!strcasecmp(i->data.c_str(), parameter.c_str()))
		{
			return std::make_pair(true, i->data);
		}
	}
        return std::make_pair(false, parameter);
}

std::string& ModeChannelBan::DelBan(User *user, std::string& dest, Channel *chan, int)
{
	if ((!user) || (!chan))
	{
		ServerInstance->Logs->Log("MODE",DEFAULT,"*** BUG *** TakeBan was given an invalid parameter");
		dest = "";
		return dest;
	}

	/* 'Clean' the mask, e.g. nick -> nick!*@* */
	ModeParser::CleanMask(dest);

	for (BanList::iterator i = chan->bans.begin(); i != chan->bans.end(); i++)
	{
		if (!strcasecmp(i->data.c_str(), dest.c_str()))
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnDelBan,OnDelBan(user, chan, dest));
			if (MOD_RESULT)
			{
				dest = "";
				return dest;
			}
			chan->bans.erase(i);
			return dest;
		}
	}
	dest = "";
	return dest;
}

