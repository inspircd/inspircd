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

	LusersCounters()
		: max_local(ServerInstance->Users->LocalUserCount())
		, max_global(ServerInstance->Users->RegisteredUserCount())
		, invisible(ServerInstance->Users->ModeCount('i'))
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

/** Handle /LUSERS. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
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
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
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
	ProtoServerList serverlist;
	ServerInstance->PI->GetServerList(serverlist);
	unsigned int n_serv = serverlist.size();
	unsigned int n_local_servs = 0;
	for(ProtoServerList::iterator i = serverlist.begin(); i != serverlist.end(); ++i)
	{
		if (i->parentname == ServerInstance->Config->ServerName)
			n_local_servs++;
	}
	// fix for default GetServerList not returning us
	if (!n_serv)
		n_serv = 1;

	counters.UpdateMaxUsers();

	user->WriteNumeric(251, "%s :There are %d users and %d invisible on %d servers",user->nick.c_str(),
			n_users - counters.invisible, counters.invisible, n_serv);

	if (ServerInstance->Users->OperCount())
		user->WriteNumeric(252, "%s %d :operator(s) online",user->nick.c_str(),ServerInstance->Users->OperCount());

	if (ServerInstance->Users->UnregisteredUserCount())
		user->WriteNumeric(253, "%s %d :unknown connections",user->nick.c_str(),ServerInstance->Users->UnregisteredUserCount());

	user->WriteNumeric(254, "%s %ld :channels formed",user->nick.c_str(),ServerInstance->ChannelCount());
	user->WriteNumeric(255, "%s :I have %d clients and %d servers",user->nick.c_str(),ServerInstance->Users->LocalUserCount(),n_local_servs);
	user->WriteNumeric(265, "%s :Current Local Users: %d  Max: %d", user->nick.c_str(), ServerInstance->Users->LocalUserCount(), counters.max_local);
	user->WriteNumeric(266, "%s :Current Global Users: %d  Max: %d", user->nick.c_str(), n_users, counters.max_global);

	return CMD_SUCCESS;
}

class InvisibleWatcher : public ModeWatcher
{
	unsigned int& invisible;
public:
	InvisibleWatcher(Module* mod, unsigned int& Invisible)
		: ModeWatcher(mod, 'i', MODETYPE_USER), invisible(Invisible)
	{
	}

	void AfterMode(User* source, User* dest, Channel* channel, const std::string& parameter, bool adding, ModeType type)
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
	LusersCounters counters;
	CommandLusers cmd;
	InvisibleWatcher mw;

 public:
	ModuleLusers()
		: cmd(this, counters), mw(this, counters.invisible)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
		Implementation events[] = { I_OnPostConnect, I_OnUserQuit };
		ServerInstance->Modules->Attach(events, this, sizeof(events)/sizeof(Implementation));
		ServerInstance->Modes->AddModeWatcher(&mw);
	}

	void OnPostConnect(User* user)
	{
		counters.UpdateMaxUsers();
		if (user->IsModeSet('i'))
			counters.invisible++;
	}

	void OnUserQuit(User* user, const std::string& message, const std::string& oper_message)
	{
		if (user->IsModeSet('i'))
			counters.invisible--;
	}

	~ModuleLusers()
	{
		ServerInstance->Modes->DelModeWatcher(&mw);
	}

	Version GetVersion()
	{
		return Version("LUSERS", VF_VENDOR | VF_CORE);
	}
};

MODULE_INIT(ModuleLusers)
