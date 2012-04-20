/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2004, 2006 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
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

/* $ModDesc: Provides channel mode +L (limit redirection) */

/** Handle channel mode +L
 */
class Redirect : public ModeHandler
{
 public:
	Redirect(InspIRCd* Instance) : ModeHandler(Instance, 'L', 1, 0, false, MODETYPE_CHANNEL, false) { }

	ModePair ModeSet(User* source, User* dest, Channel* channel, const std::string &parameter)
	{
		if (channel->IsModeSet('L'))
			return std::make_pair(true, channel->GetModeParameter('L'));
		else
			return std::make_pair(false, parameter);
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool server)
	{
		if (adding)
		{
			if (IS_LOCAL(source))
			{
				if (!ServerInstance->IsChannel(parameter.c_str(), ServerInstance->Config->Limits.ChanMax))
				{
					source->WriteNumeric(403, "%s %s :Invalid channel name", source->nick.c_str(), parameter.c_str());
					parameter.clear();
					return MODEACTION_DENY;
				}
			}

			if (IS_LOCAL(source) && !IS_OPER(source) && !server)
			{
				Channel* c = ServerInstance->FindChan(parameter);
				if (!c)
				{
					source->WriteNumeric(690, "%s :Target channel %s must exist to be set as a redirect.",source->nick.c_str(),parameter.c_str());
					parameter.clear();
					return MODEACTION_DENY;
				}
				else if (c->GetStatus(source) < STATUS_OP)
				{
					source->WriteNumeric(690, "%s :You must be opped on %s to set it as a redirect.",source->nick.c_str(),parameter.c_str());
					parameter.clear();
					return MODEACTION_DENY;
				}
			}

			if (channel->GetModeParameter('L') == parameter)
				return MODEACTION_DENY;
			/*
			 * We used to do some checking for circular +L here, but there is no real need for this any more especially as we
			 * now catch +L looping in PreJoin. Remove it, since O(n) logic makes me sad, and we catch it anyway. :) -- w00t
			 */
			channel->SetModeParam('L', parameter);
			return MODEACTION_ALLOW;
		}
		else
		{
			if (channel->IsModeSet('L'))
			{
				channel->SetModeParam('L', "");
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;

	}
};

class ModuleRedirect : public Module
{

	Redirect* re;

 public:

	ModuleRedirect(InspIRCd* Me)
		: Module(Me)
	{

		re = new Redirect(ServerInstance);
		if (!ServerInstance->Modes->AddMode(re))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnUserPreJoin };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}


	virtual int OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		if (chan)
		{
			if (chan->IsModeSet('L') && chan->modes[CM_LIMIT])
			{
				if (chan->GetUserCounter() >= atoi(chan->GetModeParameter('l').c_str()))
				{
					std::string channel = chan->GetModeParameter('L');

					/* sometimes broken ulines can make circular or chained +L, avoid this */
					Channel* destchan = NULL;
					destchan = ServerInstance->FindChan(channel);
					if (destchan && destchan->IsModeSet('L'))
					{
						user->WriteNumeric(470, "%s %s * :You may not join this channel. A redirect is set, but you may not be redirected as it is a circular loop.", user->nick.c_str(), cname);
						return 1;
					}

					user->WriteNumeric(470, "%s %s %s :You may not join this channel, so you are automatically being transferred to the redirect channel.", user->nick.c_str(), cname, channel.c_str());
					Channel::JoinUser(ServerInstance, user, channel.c_str(), false, "", false, ServerInstance->Time());
					return 1;
				}
			}
		}
		return 0;
	}

	virtual ~ModuleRedirect()
	{
		ServerInstance->Modes->DelMode(re);
		delete re;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleRedirect)
