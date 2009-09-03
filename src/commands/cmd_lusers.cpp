/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

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
 public:
	/** Constructor for lusers.
	 */
	CommandLusers (InspIRCd* Instance, Module* parent) : Command(Instance,parent,"LUSERS",0,0) { }
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
	// this lusers command shows one server at all times because
	// a protocol module must override it to show those stats.
	user->WriteNumeric(251, "%s :There are %d users and %d invisible on 1 server",user->nick.c_str(),ServerInstance->Users->UserCount()-ServerInstance->Users->ModeCount('i'),ServerInstance->Users->ModeCount('i'));
	if (ServerInstance->Users->OperCount())
		user->WriteNumeric(252, "%s %d :operator(s) online",user->nick.c_str(),ServerInstance->Users->OperCount());
	if (ServerInstance->Users->UnregisteredUserCount())
		user->WriteNumeric(253, "%s %d :unknown connections",user->nick.c_str(),ServerInstance->Users->UnregisteredUserCount());
	if (ServerInstance->ChannelCount())
		user->WriteNumeric(254, "%s %ld :channels formed",user->nick.c_str(),ServerInstance->ChannelCount());
	if (ServerInstance->Users->LocalUserCount())
		user->WriteNumeric(255, "%s :I have %d clients and 0 servers",user->nick.c_str(),ServerInstance->Users->LocalUserCount());

	return CMD_SUCCESS;
}


COMMAND_INIT(CommandLusers)
