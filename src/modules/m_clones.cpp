/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
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

typedef std::vector<User*> UList;
typedef std::map<irc::sockets::cidr_mask, UList> CloneList;

class UserTracker : public Timer
{
	bool tracking;
	unsigned long timeout;
	CloneList cl;

public:
	UserTracker()
		: Timer(timeout, false), tracking(false), timeout(0)
	{
	}

	bool Tick(time_t) CXX11_OVERRIDE
	{
		StopTracking();
		return false;
	}

	void SetTimeout(unsigned long t)
	{
		timeout = t;
	}

	const CloneList& GetMap()
	{
		if (tracking)
		{
			SetInterval(timeout);
			return cl;
		}

		if (timeout)
		{
			tracking = true;
			SetInterval(timeout);
		}

		for (user_hash::const_iterator it = ServerInstance->Users->clientlist.begin(); it != ServerInstance->Users->clientlist.end(); ++it)
		{
			User* user = it->second;
			AddClient(user);
		}
		return cl;
	}

	void StopTracking()
	{
		cl.clear();
		tracking = false;
	}

	void AddClient(User* user)
	{
		if (!tracking)
			return;

		UList& counts = cl[user->GetCIDRMask()];
		counts.push_back(user);
	}

	void DelClient(User* user)
	{
		if (!tracking)
			return;

		CloneList::iterator i = cl.find(user->GetCIDRMask());
		if (i == cl.end())
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Potential bug. CIDR was not found in clone map upon removal.");
			StopTracking();
			return;
		}

		UList& counts = i->second;
		if (!stdalgo::vector::swaperase(counts, user))
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Potential bug. User was not found in clone map upon removal.");
			StopTracking();
			return;
		}

		// If there's no more users from this IP, remove entry from the map
		if (counts.empty())
			cl.erase(i);
	}
};

/** Handle /CLONES
 */
class CommandClones : public Command
{
	UserTracker& utrack;

 public:
	CommandClones(Module* Creator, UserTracker& ut)
		: Command(Creator,"CLONES", 1), utrack(ut)
	{
		flags_needed = 'o'; syntax = "<limit>";
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		std::string clonesstr = "304 " + user->nick + " :CLONES";

		unsigned long limit = atoi(parameters[0].c_str());

		/*
		 * Syntax of a /clones reply:
		 *  :server.name 304 target :CLONES START
		 *  :server.name 304 target :CLONES <count> <ip>
		 *  :server.name 304 target :CLONES END
		 */

		user->WriteServ(clonesstr + " START");

		/* hostname or other */
		const CloneList& clonemap = utrack.GetMap();
		for (CloneList::const_iterator i = clonemap.begin(); i != clonemap.end(); ++i)
		{
			const UList& counts = i->second;
			if (counts.size() >= limit)
			{
				std::string nicklist;
				for (UList::const_iterator it = counts.begin(); it != counts.end(); ++it)
					nicklist += (*it)->nick + ", ";
				nicklist.erase(nicklist.size() - 2, 2);

				user->WriteServ(clonesstr + " " + ConvToStr(counts.size()) + " " + i->first.str() + " " + nicklist);
			}
		}

		user->WriteServ(clonesstr + " END");

		return CMD_SUCCESS;
	}
};

class ModuleClones : public Module
{
	UserTracker utrack;
	CommandClones cmd;

 public:
	ModuleClones() : cmd(this, utrack)
	{
		utrack.SetTimeout(60*5);
	}

	void OnPostConnect(User* user) CXX11_OVERRIDE
	{
		utrack.AddClient(user);
	}

	void OnUserQuit(User* user, const std::string& message, const std::string& oper_message) CXX11_OVERRIDE
	{
		utrack.DelClient(user);
	}

	void ReadConfig(ConfigStatus&) CXX11_OVERRIDE
	{
		utrack.StopTracking();
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides the /CLONES command to retrieve information on clones.", VF_VENDOR);
	}
};

MODULE_INIT(ModuleClones)
