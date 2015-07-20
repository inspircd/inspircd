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

/* $ModAuthor: Attila Molnar */
/* $ModAuthorMail: attilamolnar@hush.com */
/* $ModDesc: Implements IRC v3.2 cap-notify */
/* $ModDepends: core 2.0 */

#include "inspircd.h"
#include "m_cap.h"

class ModuleCapNotify : public Module
{
	GenericCap cap;
	bool inited;
	std::vector<std::string> currentcaps;

	void ListCaps(std::vector<std::string>& list)
	{
		CapEvent ev(this, NULL, CapEvent::CAPEVENT_LS);
		ev.Send();
		list.swap(ev.wanted);
		std::sort(list.begin(), list.end());
	}

	void SendAllWithCapNotify(const std::string& todel, const std::string& toadd)
	{
		const LocalUserList& locallist = ServerInstance->Users->local_users;
		for (LocalUserList::const_iterator i = locallist.begin(); i != locallist.end(); ++i)
		{
			LocalUser* user = *i;
			if (!cap.ext.get(user))
				continue;

			if (!toadd.empty())
				user->WriteServ("CAP %s NEW :%s", user->nick.c_str(), toadd.c_str());

			if (!todel.empty())
				user->WriteServ("CAP %s DEL :%s", user->nick.c_str(), todel.c_str());
		}
	}

	static std::string SetDiff(const std::vector<std::string>& list1, const std::vector<std::string>& list2)
	{
		std::vector<std::string> diff(list1.size());
		std::vector<std::string>::iterator lastit = std::set_difference(list1.begin(), list1.end(), list2.begin(), list2.end(), diff.begin());
		return irc::stringjoiner(" ", diff, 0, lastit - diff.begin() - 1).GetJoined();
	}

 public:
	ModuleCapNotify()
		: cap(this, "cap-notify")
		, inited(false)
	{
	}

	void init()
	{
		Implementation eventlist[] = { I_OnEvent, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void OnCleanup(int target_type, void* item)
	{
		// If we're being unloaded, send a CAP DEL (once) to users having this cap on
		if (inited)
		{
			inited = false;
			SendAllWithCapNotify(cap.cap, std::string());
		}
	}

	void On005Numeric(std::string& output)
	{
		// Rebuild CAP list whenever a module is loaded or unloaded
		std::vector<std::string> newcaps;
		ListCaps(newcaps);
		SendAllWithCapNotify(SetDiff(currentcaps, newcaps), SetDiff(newcaps, currentcaps));
		currentcaps.swap(newcaps);
	}

	void OnEvent(Event& ev)
	{
		if (ev.source != this)
			cap.HandleEvent(ev);
	}

	void Prioritize()
	{
		// XXX: This must run after all modules have been init()ed, see m_permchannels for
		// further info on this workaround
		if (!inited)
		{
			inited = true;
			ListCaps(currentcaps);
		}
	}

	Version GetVersion()
	{
		return Version("Implements IRC v3.2 cap-notify");
	}
};

MODULE_INIT(ModuleCapNotify)
