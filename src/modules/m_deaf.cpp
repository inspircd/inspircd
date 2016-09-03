/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2006, 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006-2007 Dennis Friis <peavey@inspircd.org>
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

/** User mode +d - filter out channel messages and channel notices
 */
class User_d : public ModeHandler
{
 public:
	User_d(Module* Creator) : ModeHandler(Creator, "deaf", 'd', PARAM_NONE, MODETYPE_USER) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding == dest->IsModeSet(this))
			return MODEACTION_DENY;

		if (adding)
			dest->WriteNotice("*** You have enabled usermode +d, deaf mode. This mode means you WILL NOT receive any messages from any channels you are in. If you did NOT mean to do this, use /mode " + dest->nick + " -d.");

		dest->SetMode(this, adding);
		return MODEACTION_ALLOW;
	}
};

class ModuleDeaf : public Module
{
	User_d m1;
	std::string deaf_bypasschars;
	std::string deaf_bypasschars_uline;

 public:
	ModuleDeaf()
		: m1(this)
	{
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("deaf");
		deaf_bypasschars = tag->getString("bypasschars");
		deaf_bypasschars_uline = tag->getString("bypasscharsuline");
	}

	ModResult OnUserPreMessage(User* user, void* dest, int target_type, std::string& text, char status, CUList& exempt_list, MessageType msgtype) CXX11_OVERRIDE
	{
		if (target_type != TYPE_CHANNEL)
			return MOD_RES_PASSTHRU;

		Channel* chan = static_cast<Channel*>(dest);
		bool is_bypasschar = (deaf_bypasschars.find(text[0]) != std::string::npos);
		bool is_bypasschar_uline = (deaf_bypasschars_uline.find(text[0]) != std::string::npos);

		/*
		 * If we have no bypasschars_uline in config, and this is a bypasschar (regular)
		 * Than it is obviously going to get through +d, no build required
		 */
		if (deaf_bypasschars_uline.empty() && is_bypasschar)
			return MOD_RES_PASSTHRU;

		const Channel::MemberMap& ulist = chan->GetUsers();
		for (Channel::MemberMap::const_iterator i = ulist.begin(); i != ulist.end(); ++i)
		{
			/* not +d ? */
			if (!i->first->IsModeSet(m1))
				continue; /* deliver message */
			/* matched both U-line only and regular bypasses */
			if (is_bypasschar && is_bypasschar_uline)
				continue; /* deliver message */

			bool is_a_uline = i->first->server->IsULine();
			/* matched a U-line only bypass */
			if (is_bypasschar_uline && is_a_uline)
				continue; /* deliver message */
			/* matched a regular bypass */
			if (is_bypasschar && !is_a_uline)
				continue; /* deliver message */

			/* don't deliver message! */
			exempt_list.insert(i->first);
		}

		return MOD_RES_PASSTHRU;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides usermode +d to block channel messages and channel notices", VF_VENDOR);
	}
};

MODULE_INIT(ModuleDeaf)
