/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "protocol.h"

#ifndef __CMD_LUSERS_H__
#define __CMD_LUSERS_H__

// include the common header files

#include "users.h"
#include "channels.h"

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

#endif


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


COMMAND_INIT(CommandLusers)
