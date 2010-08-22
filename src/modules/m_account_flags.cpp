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
#include "account.h"

class Channel_r : public ModeHandler
{
 public:
	Channel_r(Module* Creator) : ModeHandler(Creator, "c_registered", 'r', PARAM_NONE, MODETYPE_CHANNEL) { fixed_letter = false; }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		// only a u-lined server may add or remove the +r mode.
		if (!IS_LOCAL(source) || ServerInstance->ULine(source->nick.c_str()) || ServerInstance->ULine(source->server))
		{
			// Only change the mode if it's not redundant
			if ((adding && !channel->IsModeSet(this)) || (!adding && channel->IsModeSet(this)))
			{
				channel->SetMode(this,adding);
				return MODEACTION_ALLOW;
			}

			return MODEACTION_DENY;
		}
		else
		{
			source->WriteNumeric(500, "%s :Only a server may modify the +r channel mode", source->nick.c_str());
			return MODEACTION_DENY;
		}
	}
};

/** User mode +r - mark a user as identified
 */
class User_r : public ModeHandler
{

 public:
	User_r(Module* Creator) : ModeHandler(Creator, "u_registered", 'r', PARAM_NONE, MODETYPE_USER) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (!IS_LOCAL(source) || ServerInstance->ULine(source->nick.c_str()) || ServerInstance->ULine(source->server))
		{
			if ((adding && !dest->IsModeSet('r')) || (!adding && dest->IsModeSet('r')))
			{
				dest->SetMode('r',adding);
				return MODEACTION_ALLOW;
			}
			return MODEACTION_DENY;
		}
		else
		{
			source->WriteNumeric(500, "%s :Only a server may modify the +r user mode", source->nick.c_str());
			return MODEACTION_DENY;
		}
	}
};

class ModuleAccountFlags : public Module
{
	Channel_r mcr;
	User_r mur;
 public:
	ModuleAccountFlags() : mcr(this), mur(this) {}

	void init()
	{
		ServerInstance->Modules->AddService(mcr);
		ServerInstance->Modules->AddService(mur);
		ServerInstance->Modules->Attach(I_OnWhois, this);
		ServerInstance->Modules->Attach(I_OnUserPostNick, this);
	}

	void OnWhois(User* source, User* dest)
	{
		if (dest->IsModeSet('r'))
		{
			/* user is registered */
			ServerInstance->SendWhoisLine(source, dest, 307, "%s %s :is a registered nick", source->nick.c_str(), dest->nick.c_str());
		}
	}

	void OnUserPostNick(User* user, const std::string &oldnick)
	{
		/* On nickchange, if they have +r, remove it */
		if (user->IsModeSet('r') && irc::string(user->nick) != irc::string(oldnick))
		{
			std::vector<std::string> modechange;
			modechange.push_back(user->nick);
			modechange.push_back("-r");
			ServerInstance->SendMode(modechange, ServerInstance->FakeClient);
		}
	}

	Version GetVersion()
	{
		return Version("Povides support for modes related to accounts.", VF_VENDOR);
	}
};

MODULE_INIT(ModuleAccountFlags)
