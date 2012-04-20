/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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

/* $ModDesc: Implements extban +b m: - mute bans */

class ModuleQuietBan : public Module
{
 private:
 public:
	ModuleQuietBan(InspIRCd* Me) : Module(Me)
	{
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	virtual ~ModuleQuietBan()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$",VF_COMMON|VF_VENDOR,API_VERSION);
	}

	virtual int OnUserPreMessage(User *user, void *dest, int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (!IS_LOCAL(user))
			return 0;

		if (target_type == TYPE_CHANNEL)
		{
			if (((Channel *)dest)->GetExtBanStatus(user, 'm') < 0)
			{
				user->WriteNumeric(404, "%s %s :Cannot send to channel (you're muted)",user->nick.c_str(), ((Channel *)dest)->name.c_str());
				return 1;
			}
		}

		return 0;
	}

	virtual int OnUserPreNotice(User *user, void *dest, int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (!IS_LOCAL(user))
			return 0;

		if (target_type == TYPE_CHANNEL)
		{
			if (((Channel *)dest)->GetExtBanStatus(user, 'm') < 0)
			{
				user->WriteNumeric(404, "%s %s :Cannot send to channel (you're muted)",user->nick.c_str(), ((Channel *)dest)->name.c_str());
				return 1;
			}
		}

		return 0;
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('m');
	}
};


MODULE_INIT(ModuleQuietBan)

