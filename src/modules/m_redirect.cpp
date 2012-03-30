/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Provides channel mode +L (limit redirection) and usermode +L (no forced redirection) */

/** Handle channel mode +L
 */
class Redirect : public ModeHandler
{
 public:
	Redirect(Module* Creator) : ModeHandler(Creator, "redirect", 'L', PARAM_SETONLY, MODETYPE_CHANNEL) { fixed_letter = false; }

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
				else if (c->GetAccessRank(source) < OP_VALUE)
				{
					source->WriteNumeric(690, "%s :You must be opped on %s to set it as a redirect.",source->nick.c_str(),parameter.c_str());
					parameter.clear();
					return MODEACTION_DENY;
				}
			}

			if (channel->GetModeParameter(this) == parameter)
				return MODEACTION_DENY;
			/*
			 * We used to do some checking for circular +L here, but there is no real need for this any more especially as we
			 * now catch +L looping in PreJoin. Remove it, since O(n) logic makes me sad, and we catch it anyway. :) -- w00t
			 */
			channel->SetModeParam(this, parameter);
			return MODEACTION_ALLOW;
		}
		else
		{
			if (channel->IsModeSet(this))
			{
				channel->SetModeParam(this, "");
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;

	}
};

class RedirectUMode : public ModeHandler
{
	public:
		RedirectUMode(Module* Creator) : ModeHandler(Creator, "redirect_u", 'L', PARAM_NONE, MODETYPE_USER) {}

		ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &paramater, bool adding)
		{
			if (adding)
			{
				if (!dest->IsModeSet('L'))
				{
					dest->SetMode('L', true);
					return MODEACTION_ALLOW;
				}
			}
			else
			{
				if (dest->IsModeSet('L'))
				{
					dest->SetMode('L', false);
					return MODEACTION_ALLOW;
				}
			}

			return MODEACTION_DENY;
		}
};

class ModuleRedirect : public Module
{

	Redirect re;
	RedirectUMode re_u;

 public:

	ModuleRedirect() : re(this), re_u(this) {}

	void init()
	{
		ServerInstance->Modules->AddService(re);
		ServerInstance->Modules->AddService(re_u);
		Implementation eventlist[] = { I_OnPermissionCheck };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void OnPermissionCheck(PermissionData& perm)
	{
		// we want a DENIED join action
		if (!perm.chan || perm.result != MOD_RES_DENY || perm.name != "join")
			return;
		// already in a redirect, don't double-redirect
		if (ServerInstance->RedirectJoin.get(perm.source))
			return;
		// not +L
		if (!perm.chan->IsModeSet(&re))
			return;

		// ok, now actually do the redirect
		std::string channel = perm.chan->GetModeParameter(&re);

		// If umode L is set they don't want auto-redirection.
		if (perm.source->IsModeSet('L'))
		{
			perm.ErrorNumeric(470, "%s %s :Trying to redirect you from %s to %s, stopped because of +L",
				perm.chan->name.c_str(), channel.c_str(), perm.chan->name.c_str(), channel.c_str());
		}
		else
		{
			perm.ErrorNumeric(470, "%s %s :You have been transferred by a channel redirection from %s to %s.",
				perm.chan->name.c_str(), channel.c_str(), perm.chan->name.c_str(), channel.c_str());
			ServerInstance->RedirectJoin.set(perm.source, 1);
			Channel::JoinUser(perm.source, channel.c_str(), false, "", false, ServerInstance->Time());
			ServerInstance->RedirectJoin.set(perm.source, 0);
		}
	}

	virtual ~ModuleRedirect()
	{
	}

	void Prioritize()
	{
		ServerInstance->Modules->SetPriority(this, I_OnPermissionCheck, PRIORITY_LAST);
	}

	virtual Version GetVersion()
	{
		return Version("Provides channel mode +L (channel redirection) and user mode +L (no forced redirection)", VF_VENDOR);
	}
};

MODULE_INIT(ModuleRedirect)
