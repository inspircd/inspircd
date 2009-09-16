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

/** Handle /INVITE. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandInvite : public Command
{
 public:
	/** Constructor for invite.
	 */
	CommandInvite ( Module* parent) : Command(parent,"INVITE", 0, 0) { Penalty = 4; syntax = "[<nick> <channel>]"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

/** Handle /INVITE
 */
CmdResult CommandInvite::Handle (const std::vector<std::string>& parameters, User *user)
{
	ModResult MOD_RESULT;

	if (parameters.size() == 2 || parameters.size() == 3)
	{
		User* u = ServerInstance->FindNick(parameters[0]);
		Channel* c = ServerInstance->FindChan(parameters[1]);
		time_t timeout = 0;
		if (parameters.size() == 3)
		{
			if (IS_LOCAL(user))
				timeout = ServerInstance->Time() + ServerInstance->Duration(parameters[2]);
			else
				timeout = ConvToInt(parameters[2]);
		}

		if ((!c) || (!u))
		{
			user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel",user->nick.c_str(), c ? parameters[0].c_str() : parameters[1].c_str());
			return CMD_FAILURE;
		}

		if (c->HasUser(u))
	 	{
	 		user->WriteNumeric(ERR_USERONCHANNEL, "%s %s %s :is already on channel",user->nick.c_str(),u->nick.c_str(),c->name.c_str());
	 		return CMD_FAILURE;
		}

		if ((IS_LOCAL(user)) && (!c->HasUser(user)))
	 	{
			user->WriteNumeric(ERR_NOTONCHANNEL, "%s %s :You're not on that channel!",user->nick.c_str(), c->name.c_str());
	  		return CMD_FAILURE;
		}

		FIRST_MOD_RESULT(ServerInstance, OnUserPreInvite, MOD_RESULT, (user,u,c,timeout));

		if (MOD_RESULT == MOD_RES_DENY)
		{
			return CMD_FAILURE;
		}
		else if (MOD_RESULT == MOD_RES_PASSTHRU)
		{
			if (IS_LOCAL(user))
			{
				int rank = c->GetPrefixValue(user);
				if (rank < HALFOP_VALUE)
				{
					user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You must be a channel %soperator",
						user->nick.c_str(), c->name.c_str(), rank >= HALFOP_VALUE ? "" : "half-");
					return CMD_FAILURE;
				}
			}
		}

		u->InviteTo(c->name.c_str(), timeout);
		u->WriteFrom(user,"INVITE %s :%s",u->nick.c_str(),c->name.c_str());
		user->WriteNumeric(RPL_INVITING, "%s %s %s",user->nick.c_str(),u->nick.c_str(),c->name.c_str());
		switch (ServerInstance->Config->AnnounceInvites)
		{
			case ServerConfig::INVITE_ANNOUNCE_ALL:
				c->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :*** %s invited %s into the channel", c->name.c_str(), user->nick.c_str(), u->nick.c_str());
			break;
			case ServerConfig::INVITE_ANNOUNCE_OPS:
				c->WriteAllExceptSender(user, true, '@', "NOTICE %s :*** %s invited %s into the channel", c->name.c_str(), user->nick.c_str(), u->nick.c_str());
			break;
			case ServerConfig::INVITE_ANNOUNCE_DYNAMIC:
				if (c->IsModeSet('i'))
					c->WriteAllExceptSender(user, true, '@', "NOTICE %s :*** %s invited %s into the channel", c->name.c_str(), user->nick.c_str(), u->nick.c_str());
				else
					c->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :*** %s invited %s into the channel", c->name.c_str(), user->nick.c_str(), u->nick.c_str());
			break;
			default:
				/* Nobody */
			break;
		}
		FOREACH_MOD(I_OnUserInvite,OnUserInvite(user,u,c,timeout));
	}
	else
	{
		// pinched from ircu - invite with not enough parameters shows channels
		// youve been invited to but haven't joined yet.
		InvitedList* il = user->GetInviteList();
		for (InvitedList::iterator i = il->begin(); i != il->end(); i++)
		{
			user->WriteNumeric(RPL_INVITELIST, "%s :%s",user->nick.c_str(),i->first.c_str());
		}
		user->WriteNumeric(RPL_ENDOFINVITELIST, "%s :End of INVITE list",user->nick.c_str());
	}
	return CMD_SUCCESS;
}


COMMAND_INIT(CommandInvite)
