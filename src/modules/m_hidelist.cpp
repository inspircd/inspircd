/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
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

class ListWatcher : public ModeWatcher
{
 public:
	ListWatcher(Module* mod, const std::string& modename)
		: ModeWatcher(mod, modename, MODETYPE_CHANNEL)
	{
	}

	bool BeforeMode(User* user, User* destuser, Channel* chan, std::string& param, bool adding)
	{
		// Only handle listmode list requests
		if (!param.empty())
			return true;

		// If the user requesting the list is a member of the channel see if they have the
		// rank required to view the list
		Membership* memb = chan->GetUser(user);
		if ((memb) && (memb->getRank() >= HALFOP_VALUE))
			return true;

		if (user->HasPrivPermission("channels/auspex"))
			return true;

		user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s :You do not have access to view the %s list", chan->name.c_str(), GetModeName().c_str());
		return false;
	}
};

class ModuleHideList : public Module
{
	std::vector<ListWatcher*> watchers;

 public:
	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		stdalgo::delete_all(watchers);
		watchers.clear();

		ConfigTagList tags = ServerInstance->Config->ConfTags("hidelist");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			ConfigTag* tag = i->second;
			std::string modename = tag->getString("mode");
			watchers.push_back(new ListWatcher(this, modename));
		}
	}

	~ModuleHideList()
	{
		stdalgo::delete_all(watchers);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides support for hiding the list of listmodes", VF_VENDOR);
	}
};

MODULE_INIT(ModuleHideList)
