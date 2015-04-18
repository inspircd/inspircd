/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2012 Shawn Smith <ShawnSmith0828@gmail.com>
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

/** Handle channel mode +L
 */
class Redirect : public ParamMode<Redirect, LocalStringExt>
{
 public:
	Redirect(Module* Creator)
		: ParamMode<Redirect, LocalStringExt>(Creator, "redirect", 'L') { }

	ModeAction OnSet(User* source, Channel* channel, std::string& parameter)
	{
		if (IS_LOCAL(source))
		{
			if (!ServerInstance->IsChannel(parameter))
			{
				source->WriteNumeric(ERR_NOSUCHCHANNEL, "%s :Invalid channel name", parameter.c_str());
				return MODEACTION_DENY;
			}
		}

		if (IS_LOCAL(source) && !source->IsOper())
		{
			Channel* c = ServerInstance->FindChan(parameter);
			if (!c)
			{
				source->WriteNumeric(690, ":Target channel %s must exist to be set as a redirect.",parameter.c_str());
				return MODEACTION_DENY;
			}
			else if (c->GetPrefixValue(source) < OP_VALUE)
			{
				source->WriteNumeric(690, ":You must be opped on %s to set it as a redirect.",parameter.c_str());
				return MODEACTION_DENY;
			}
		}

		/*
		 * We used to do some checking for circular +L here, but there is no real need for this any more especially as we
		 * now catch +L looping in PreJoin. Remove it, since O(n) logic makes me sad, and we catch it anyway. :) -- w00t
		 */
		ext.set(channel, parameter);
		return MODEACTION_ALLOW;
	}

	void SerializeParam(Channel* chan, const std::string* str, std::string& out)
	{
		out += *str;
	}
};

/** Handles usermode +L to stop forced redirection and print an error.
*/
class AntiRedirect : public SimpleUserModeHandler
{
	public:
		AntiRedirect(Module* Creator) : SimpleUserModeHandler(Creator, "antiredirect", 'L')
		{
		}
};

class ModuleRedirect : public Module
{
	Redirect re;
	AntiRedirect re_u;
	ChanModeReference limitmode;

 public:
	ModuleRedirect()
		: re(this)
		, re_u(this)
		, limitmode(this, "limit")
	{
	}

	void init() CXX11_OVERRIDE
	{
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven) CXX11_OVERRIDE
	{
		if (chan)
		{
			if (chan->IsModeSet(re) && chan->IsModeSet(limitmode))
			{
				if (chan->GetUserCounter() >= ConvToInt(chan->GetModeParameter(limitmode)))
				{
					const std::string& channel = *re.ext.get(chan);

					/* sometimes broken ulines can make circular or chained +L, avoid this */
					Channel* destchan = ServerInstance->FindChan(channel);
					if (destchan && destchan->IsModeSet(re))
					{
						user->WriteNumeric(470, "%s * :You may not join this channel. A redirect is set, but you may not be redirected as it is a circular loop.", cname.c_str());
						return MOD_RES_DENY;
					}

					if (user->IsModeSet(re_u))
					{
						user->WriteNumeric(470, "%s %s :Force redirection stopped.", cname.c_str(), channel.c_str());
						return MOD_RES_DENY;
					}
					else
					{
						user->WriteNumeric(470, "%s %s :You may not join this channel, so you are automatically being transferred to the redirect channel.", cname.c_str(), channel.c_str());
						Channel::JoinUser(user, channel);
						return MOD_RES_DENY;
					}
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides channel mode +L (limit redirection) and user mode +L (no forced redirection)", VF_VENDOR);
	}
};

MODULE_INIT(ModuleRedirect)
