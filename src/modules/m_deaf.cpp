/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2006, 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006-2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2012 satmd <satmd@satmd.dyndns.org>
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

// User mode +d - filter out channel messages and channel notices
class DeafMode : public ModeHandler
{
 public:
	DeafMode(Module* Creator) : ModeHandler(Creator, "deaf", 'd', PARAM_NONE, MODETYPE_USER) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string& parameter, bool adding) CXX11_OVERRIDE
	{
		if (adding == dest->IsModeSet(this))
			return MODEACTION_DENY;

		if (adding)
			dest->WriteNotice("*** You have enabled user mode +d, deaf mode. This mode means you WILL NOT receive any messages from any channels you are in. If you did NOT mean to do this, use /mode " + dest->nick + " -d.");

		dest->SetMode(this, adding);
		return MODEACTION_ALLOW;
	}
};

// User mode +D - filter out user messages and user notices
class PrivDeafMode : public ModeHandler
{
 public:
	PrivDeafMode(Module* Creator) : ModeHandler(Creator, "privdeaf", 'D', PARAM_NONE, MODETYPE_USER)
	{
		if (!ServerInstance->Config->ConfValue("deaf")->getBool("enableprivdeaf"))
			DisableAutoRegister();
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string& parameter, bool adding) CXX11_OVERRIDE
	{
		if (adding == dest->IsModeSet(this))
			return MODEACTION_DENY;

		if (adding)
			dest->WriteNotice("*** You have enabled user mode +D, private deaf mode. This mode means you WILL NOT receive any messages and notices from any nicks. If you did NOT mean to do this, use /mode " + dest->nick + " -D.");

		dest->SetMode(this, adding);
		return MODEACTION_ALLOW;
	}
};

class ModuleDeaf : public Module
{
	DeafMode deafmode;
	PrivDeafMode privdeafmode;
	std::string deaf_bypasschars;
	std::string deaf_bypasschars_uline;
	bool privdeafuline;

 public:
	ModuleDeaf()
		: deafmode(this)
		, privdeafmode(this)
	{
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("deaf");
		deaf_bypasschars = tag->getString("bypasschars");
		deaf_bypasschars_uline = tag->getString("bypasscharsuline");
		privdeafuline = tag->getBool("privdeafuline", true);
	}

	ModResult OnUserPreMessage(User* user, const MessageTarget& target, MessageDetails& details) CXX11_OVERRIDE
	{
		switch (target.type)
		{
			case MessageTarget::TYPE_CHANNEL:
			{
				Channel* chan = target.Get<Channel>();
				bool is_bypasschar = (deaf_bypasschars.find(details.text[0]) != std::string::npos);
				bool is_bypasschar_uline = (deaf_bypasschars_uline.find(details.text[0]) != std::string::npos);

				// If we have no bypasschars_uline in config, and this is a bypasschar (regular)
				// Then it is obviously going to get through +d, no exemption list required
				if (deaf_bypasschars_uline.empty() && is_bypasschar)
					return MOD_RES_PASSTHRU;
				// If it matches both bypasschar and bypasschar_uline, it will get through.
				if (is_bypasschar && is_bypasschar_uline)
					return MOD_RES_PASSTHRU;

				const Channel::MemberMap& ulist = chan->GetUsers();
				for (Channel::MemberMap::const_iterator i = ulist.begin(); i != ulist.end(); ++i)
				{
					// not +d
					if (!i->first->IsModeSet(deafmode))
						continue;

					bool is_a_uline = i->first->server->IsULine();
					// matched a U-line only bypass
					if (is_bypasschar_uline && is_a_uline)
						continue;
					// matched a regular bypass
					if (is_bypasschar && !is_a_uline)
						continue;

					// don't deliver message!
					details.exemptions.insert(i->first);
				}
				break;
			}
			case MessageTarget::TYPE_USER:
			{
				User* targ = target.Get<User>();
				if (!targ->IsModeSet(privdeafmode))
					return MOD_RES_PASSTHRU;

				if (!privdeafuline && user->server->IsULine())
					return MOD_RES_DENY;

				if (!user->HasPrivPermission("users/ignore-privdeaf"))
					return MOD_RES_DENY;

				break;
			}
			case MessageTarget::TYPE_SERVER:
				break;
		}

		return MOD_RES_PASSTHRU;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides user modes +d and +D to block channel and user messages/notices", VF_VENDOR);
	}
};

MODULE_INIT(ModuleDeaf)
