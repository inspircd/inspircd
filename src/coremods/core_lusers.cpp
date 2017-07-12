/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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

struct LusersCounters
{
	unsigned int max_local;
	unsigned int max_global;
	unsigned int invisible;

	LusersCounters(unsigned int inv)
		: max_local(ServerInstance->Users->LocalUserCount())
		, max_global(ServerInstance->Users->RegisteredUserCount())
		, invisible(inv)
	{
	}

	inline void UpdateMaxUsers()
	{
		unsigned int current = ServerInstance->Users->LocalUserCount();
		if (current > max_local)
			max_local = current;

		current = ServerInstance->Users->RegisteredUserCount();
		if (current > max_global)
			max_global = current;
	}
};

/** Handle /LUSERS.
 */
class CommandLusers : public Command
{
	LusersCounters& counters;
 public:
	/** Constructor for lusers.
	 */
	CommandLusers(Module* parent, LusersCounters& Counters)
		: Command(parent,"LUSERS",0,0), counters(Counters)
	{ }
	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

/** Handle /LUSERS
 */
CmdResult CommandLusers::Handle (const std::vector<std::string>&, User *user)
{
	unsigned int n_users = ServerInstance->Users->RegisteredUserCount();
	ProtocolInterface::ServerList serverlist;
	ServerInstance->PI->GetServerList(serverlist);
	unsigned int n_serv = serverlist.size();
	unsigned int n_local_servs = 0;
	for (ProtocolInterface::ServerList::const_iterator i = serverlist.begin(); i != serverlist.end(); ++i)
	{
		if (i->parentname == ServerInstance->Config->ServerName)
			n_local_servs++;
	}
	// fix for default GetServerList not returning us
	if (!n_serv)
		n_serv = 1;

	counters.UpdateMaxUsers();

	user->WriteNumeric(RPL_LUSERCLIENT, InspIRCd::Format("There are %d users and %d invisible on %d servers",
			n_users - counters.invisible, counters.invisible, n_serv));

	if (ServerInstance->Users->OperCount())
		user->WriteNumeric(RPL_LUSEROP, ServerInstance->Users.OperCount(), "operator(s) online");

	if (ServerInstance->Users->UnregisteredUserCount())
		user->WriteNumeric(RPL_LUSERUNKNOWN, ServerInstance->Users.UnregisteredUserCount(), "unknown connections");

	user->WriteNumeric(RPL_LUSERCHANNELS, ServerInstance->GetChans().size(), "channels formed");
	user->WriteNumeric(RPL_LUSERME, InspIRCd::Format("I have %d clients and %d servers", ServerInstance->Users.LocalUserCount(), n_local_servs));
	user->WriteNumeric(RPL_LOCALUSERS, InspIRCd::Format("Current local users: %d  Max: %d", ServerInstance->Users.LocalUserCount(), counters.max_local));
	user->WriteNumeric(RPL_GLOBALUSERS, InspIRCd::Format("Current global users: %d  Max: %d", n_users, counters.max_global));

	return CMD_SUCCESS;
}

class InvisibleWatcher : public ModeWatcher
{
	unsigned int& invisible;
public:
	InvisibleWatcher(Module* mod, unsigned int& Invisible)
		: ModeWatcher(mod, "invisible", MODETYPE_USER), invisible(Invisible)
	{
	}

	void AfterMode(User* source, User* dest, Channel* channel, const std::string& parameter, bool adding)
	{
		if (dest->registered != REG_ALL)
			return;

		if (adding)
			invisible++;
		else
			invisible--;
	}
};

class ModuleLusers : public Module
{
	UserModeReference invisiblemode;
	LusersCounters counters;
	CommandLusers cmd;
	InvisibleWatcher mw;

	unsigned int CountInvisible()
	{
		unsigned int c = 0;
		const user_hash& users = ServerInstance->Users->GetUsers();
		for (user_hash::const_iterator i = users.begin(); i != users.end(); ++i)
		{
			User* u = i->second;
			if (u->IsModeSet(invisiblemode))
				c++;
		}
		return c;
	}

 public:
	ModuleLusers()
		: invisiblemode(this, "invisible")
		, counters(CountInvisible())
		, cmd(this, counters)
		, mw(this, counters.invisible)
	{
	}

	void OnPostConnect(User* user) CXX11_OVERRIDE
	{
		counters.UpdateMaxUsers();
		if (user->IsModeSet(invisiblemode))
			counters.invisible++;
	}

	void OnUserQuit(User* user, const std::string& message, const std::string& oper_message) CXX11_OVERRIDE
	{
		if (user->IsModeSet(invisiblemode))
			counters.invisible--;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("LUSERS", VF_VENDOR | VF_CORE);
	}
};

MODULE_INIT(ModuleLusers)
