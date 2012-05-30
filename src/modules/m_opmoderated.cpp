/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
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

/* $ModDesc: Implements channel mode +U and extban 'U' - moderator mute */

class OpModeratedMode : public SimpleChannelModeHandler
{
 public:
	OpModeratedMode(Module* Creator) : SimpleChannelModeHandler(Creator, "opmoderated", 'U') { fixed_letter = false; }
};

class ModuleOpModerated : public Module
{
	OpModeratedMode mh;
 public:
	ModuleOpModerated() : mh(this) {}

	void init()
	{
		ServerInstance->Modules->AddService(mh);
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	Version GetVersion()
	{
		return Version("Implements opmoderated channel mode +U (non-voiced messages sent to ops) and extban 'U'", VF_OPTCOMMON|VF_VENDOR);
	}

	ModResult DoMsg(User *user, void *dest, int target_type, std::string &text, char status, CUList &exempt_list, bool privmsg)
	{
		if (!IS_LOCAL(user) || target_type != TYPE_CHANNEL || status)
			return MOD_RES_PASSTHRU;

		Channel* chan = static_cast<Channel*>(dest);
		ModResult res = ServerInstance->CheckExemption(user,chan,"opmoderated");
		if (res == MOD_RES_ALLOW)
			return MOD_RES_PASSTHRU;
		if (!chan->GetExtBanStatus(user, 'U').check(!chan->IsModeSet(&mh)) && chan->GetAccessRank(user) < VOICE_VALUE)
		{
			FOREACH_MOD(I_OnText,OnText(user,chan,TYPE_CHANNEL,text,status,exempt_list));
			chan->WriteAllExcept(user, false, '@', exempt_list, "%s @%s :%s",
				privmsg ? "PRIVMSG" : "NOTICE", chan->name.c_str(), text.c_str());
			if (privmsg)
				FOREACH_MOD(I_OnUserMessage,OnUserMessage(user,chan,TYPE_CHANNEL,text,'@',exempt_list));
			else
				FOREACH_MOD(I_OnUserNotice,OnUserNotice(user,chan,TYPE_CHANNEL,text,'@',exempt_list));

			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreMessage(User *user, void *dest, int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return DoMsg(user, dest, target_type, text, status, exempt_list, true);
	}
	ModResult OnUserPreNotice(User *user, void *dest, int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return DoMsg(user, dest, target_type, text, status, exempt_list, false);
	}

	void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('U');
	}

	void Prioritize()
	{
		// since we steal the message, we should be last (let everyone else eat it first)
		ServerInstance->Modules->SetPriority(this, I_OnUserPreMessage, PRIORITY_LAST);
		ServerInstance->Modules->SetPriority(this, I_OnUserPreNotice, PRIORITY_LAST);
	}
};

MODULE_INIT(ModuleOpModerated)
