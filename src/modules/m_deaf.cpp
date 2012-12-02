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

/* $ModDesc: Provides usermode +d to block channel messages and channel notices */

/** User mode +d - filter out channel messages and channel notices
 */
class User_d : public ModeHandler
{
 public:
	User_d(Module* Creator) : ModeHandler(Creator, "deaf", 'd', PARAM_NONE, MODETYPE_USER) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!dest->IsModeSet('d'))
			{
				dest->WriteServ("NOTICE %s :*** You have enabled usermode +d, deaf mode. This mode means you WILL NOT receive any messages from any channels you are in. If you did NOT mean to do this, use /mode %s -d.", dest->nick.c_str(), dest->nick.c_str());
				dest->SetMode('d',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('d'))
			{
				dest->SetMode('d',false);
				return MODEACTION_ALLOW;
			}
		}
		return MODEACTION_DENY;
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

	void init()
	{
		ServerInstance->Modules->AddService(m1);

		OnRehash(NULL);
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual void OnRehash(User* user)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("deaf");
		deaf_bypasschars = tag->getString("bypasschars");
		deaf_bypasschars_uline = tag->getString("bypasscharsuline");
	}

	virtual ModResult OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (target_type == TYPE_CHANNEL)
		{
			Channel* chan = (Channel*)dest;
			if (chan)
				this->BuildDeafList(MSG_NOTICE, chan, user, status, text, exempt_list);
		}

		return MOD_RES_PASSTHRU;
	}

	virtual ModResult OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (target_type == TYPE_CHANNEL)
		{
			Channel* chan = (Channel*)dest;
			if (chan)
				this->BuildDeafList(MSG_PRIVMSG, chan, user, status, text, exempt_list);
		}

		return MOD_RES_PASSTHRU;
	}

	virtual void BuildDeafList(MessageType message_type, Channel* chan, User* sender, char status, const std::string &text, CUList &exempt_list)
	{
		const UserMembList *ulist = chan->GetUsers();
		bool is_a_uline;
		bool is_bypasschar, is_bypasschar_avail;
		bool is_bypasschar_uline, is_bypasschar_uline_avail;

		is_bypasschar = is_bypasschar_avail = is_bypasschar_uline = is_bypasschar_uline_avail = 0;
		if (!deaf_bypasschars.empty())
		{
			is_bypasschar_avail = 1;
			if (deaf_bypasschars.find(text[0], 0) != std::string::npos)
				is_bypasschar = 1;
		}
		if (!deaf_bypasschars_uline.empty())
		{
			is_bypasschar_uline_avail = 1;
			if (deaf_bypasschars_uline.find(text[0], 0) != std::string::npos)
				is_bypasschar_uline = 1;
		}

		/*
		 * If we have no bypasschars_uline in config, and this is a bypasschar (regular)
		 * Than it is obviously going to get through +d, no build required
		 */
		if (!is_bypasschar_uline_avail && is_bypasschar)
			return;

		for (UserMembCIter i = ulist->begin(); i != ulist->end(); i++)
		{
			/* not +d ? */
			if (!i->first->IsModeSet('d'))
				continue; /* deliver message */
			/* matched both U-line only and regular bypasses */
			if (is_bypasschar && is_bypasschar_uline)
				continue; /* deliver message */

			is_a_uline = ServerInstance->ULine(i->first->server);
			/* matched a U-line only bypass */
			if (is_bypasschar_uline && is_a_uline)
				continue; /* deliver message */
			/* matched a regular bypass */
			if (is_bypasschar && !is_a_uline)
				continue; /* deliver message */

			if (status && !strchr(chan->GetAllPrefixChars(i->first), status))
				continue;

			/* don't deliver message! */
			exempt_list.insert(i->first);
		}
	}

	virtual ~ModuleDeaf()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides usermode +d to block channel messages and channel notices", VF_VENDOR);
	}

};

MODULE_INIT(ModuleDeaf)
