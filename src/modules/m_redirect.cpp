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

/* $ModDesc: Provides channel mode +L (limit redirection) and usermode +L (no forced redirection) */

/** Handle channel mode +L
 */
class Redirect : public ModeHandler
{
 public:
	Redirect(Module* Creator) : ModeHandler(Creator, "redirect", 'L', PARAM_SETONLY, MODETYPE_CHANNEL) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
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

			if (IS_LOCAL(source) && !IS_OPER(source))
			{
				Channel* c = ServerInstance->FindChan(parameter);
				if (!c)
				{
					source->WriteNumeric(690, "%s :Target channel %s must exist to be set as a redirect.",source->nick.c_str(),parameter.c_str());
					parameter.clear();
					return MODEACTION_DENY;
				}
				else if (c->GetPrefixValue(source) < OP_VALUE)
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

/** Handles usermode +L to stop forced redirection and print an error.
*/
class AntiRedirect : public SimpleUserModeHandler
{
	public:
		AntiRedirect(Module* Creator) : SimpleUserModeHandler(Creator, "antiredirect", 'L') {}
};

class ModuleRedirect : public Module
{

	Redirect re;
	AntiRedirect re_u;
	bool UseUsermode;

 public:

	ModuleRedirect()
		: re(this), re_u(this)
	{
	}

	void init()
	{
		/* Setting this here so it isn't changable by rehasing the config later. */
		UseUsermode = ServerInstance->Config->ConfValue("redirect")->getBool("antiredirect");

		/* Channel mode */
		ServerInstance->Modules->AddService(re);

		/* Check to see if the usermode is enabled in the config */
		if (UseUsermode)
		{
			/* Log noting that this breaks compatability. */
			ServerInstance->Logs->Log("m_redirect", DEFAULT, "REDIRECT: Enabled usermode +L. This breaks linking with servers that do not have this enabled. This is disabled by default in the 2.0 branch but will be enabled in the next version.");

			/* Try to add the usermode */
			ServerInstance->Modules->AddService(re_u);
		}

		Implementation eventlist[] = { I_OnUserPreJoin };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual ModResult OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		if (chan)
		{
			if (chan->IsModeSet('L') && chan->IsModeSet('l'))
			{
				if (chan->GetUserCounter() >= ConvToInt(chan->GetModeParameter('l')))
				{
					std::string channel = chan->GetModeParameter('L');

					/* sometimes broken ulines can make circular or chained +L, avoid this */
					Channel* destchan = NULL;
					destchan = ServerInstance->FindChan(channel);
					if (destchan && destchan->IsModeSet('L'))
					{
						user->WriteNumeric(470, "%s %s * :You may not join this channel. A redirect is set, but you may not be redirected as it is a circular loop.", user->nick.c_str(), cname);
						return MOD_RES_DENY;
					}
					/* We check the bool value here to make sure we have it enabled, if we don't then
						usermode +L might be assigned to something else. */
					if (UseUsermode && user->IsModeSet('L'))
					{
						user->WriteNumeric(470, "%s %s %s :Force redirection stopped.",
						user->nick.c_str(), cname, channel.c_str());
						return MOD_RES_DENY;
					}
					else
					{
						user->WriteNumeric(470, "%s %s %s :You may not join this channel, so you are automatically being transferred to the redirect channel.", user->nick.c_str(), cname, channel.c_str());
						Channel::JoinUser(user, channel.c_str(), false, "", false, ServerInstance->Time());
						return MOD_RES_DENY;
					}
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}

	virtual ~ModuleRedirect()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides channel mode +L (limit redirection) and user mode +L (no forced redirection)", VF_VENDOR);
	}
};

MODULE_INIT(ModuleRedirect)
