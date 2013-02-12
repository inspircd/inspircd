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

/** Handle /LUSERS. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandLusers : public Command
{
	unsigned int max_local, max_global;
 public:
	/** Constructor for lusers.
	 */
	CommandLusers ( Module* parent) : Command(parent,"LUSERS",0,0),
		max_local(0), max_global(0)
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
	unsigned int n_users = ServerInstance->Users->UserCount();
	unsigned int n_invis = ServerInstance->Users->ModeCount('i');
	ProtoServerList serverlist;
	ServerInstance->PI->GetServerList(serverlist);
	int n_serv = 0;
	int n_local_servs = 0;
	for(ProtoServerList::iterator i = serverlist.begin(); i != serverlist.end(); ++i)
	{
		n_serv++;
		if (i->parentname == ServerInstance->Config->ServerName)
			n_local_servs++;
	}
	// fix for default GetServerList not returning us
	if (!n_serv)
		n_serv = 1;

	// these are updated on every connect (or /lusers invocation), which is good enough
	if (ServerInstance->Users->LocalUserCount() > max_local)
		max_local = ServerInstance->Users->LocalUserCount();
	if (n_users > max_global)
		max_global = n_users;

	user->WriteNumeric(251, "%s :There are %d users and %d invisible on %d servers",user->nick.c_str(),
			n_users-n_invis, n_invis, n_serv);

	if (ServerInstance->Users->OperCount())
		user->WriteNumeric(252, "%s %d :operator(s) online",user->nick.c_str(),ServerInstance->Users->OperCount());

	if (ServerInstance->Users->UnregisteredUserCount())
		user->WriteNumeric(253, "%s %d :unknown connections",user->nick.c_str(),ServerInstance->Users->UnregisteredUserCount());

	user->WriteNumeric(254, "%s %ld :channels formed",user->nick.c_str(),ServerInstance->ChannelCount());
	user->WriteNumeric(255, "%s :I have %d clients and %d servers",user->nick.c_str(),ServerInstance->Users->LocalUserCount(),n_local_servs);
	user->WriteNumeric(265, "%s :Current Local Users: %d  Max: %d",user->nick.c_str(),ServerInstance->Users->LocalUserCount(),max_local);
	user->WriteNumeric(266, "%s :Current Global Users: %d  Max: %d",user->nick.c_str(),n_users,max_global);

	return CMD_SUCCESS;
}

class ModuleLusers : public Module
{
	CommandLusers cmd;

 public:
	ModuleLusers()
		: cmd(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
	}

	Version GetVersion()
	{
		return Version("LUSERS", VF_VENDOR | VF_CORE);
	}
};

MODULE_INIT(ModuleLusers)
