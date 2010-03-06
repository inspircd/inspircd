/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Provides channel mode +L (limit redirection) */

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

class ModuleRedirect : public Module
{

	Redirect re;

 public:

	ModuleRedirect()
		: re(this)
	{

		if (!ServerInstance->Modes->AddMode(&re))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnUserPreJoin };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}


	virtual ModResult OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		if (chan)
		{
			if (chan->IsModeSet('L') && chan->IsModeSet('l'))
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
						return MOD_RES_DENY;
					}

					user->WriteNumeric(470, "%s %s %s :You may not join this channel, so you are automatically being transferred to the redirect channel.", user->nick.c_str(), cname, channel.c_str());
					Channel::JoinUser(user, channel.c_str(), false, "", false, ServerInstance->Time());
					return MOD_RES_DENY;
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
		return Version("Provides channel mode +L (limit redirection)", VF_VENDOR);
	}
};

MODULE_INIT(ModuleRedirect)
