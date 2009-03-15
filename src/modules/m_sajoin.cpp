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

/* $ModDesc: Provides support for unreal-style SAJOIN command */

/** Handle /SAJOIN
 */
class CommandSajoin : public Command
{
 public:
	CommandSajoin (InspIRCd* Instance) : Command(Instance,"SAJOIN", "o", 2, false, 0)
	{
		this->source = "m_sajoin.so";
		syntax = "<nick> <channel>";
		TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);
		if (dest)
		{
			if (ServerInstance->ULine(dest->server))
			{
				user->WriteNumeric(ERR_NOPRIVILEGES, "%s :Cannot use an SA command on a u-lined client",user->nick.c_str());
				return CMD_FAILURE;
			}
			if (IS_LOCAL(user) && !ServerInstance->IsChannel(parameters[1].c_str(), ServerInstance->Config->Limits.ChanMax))
			{
				/* we didn't need to check this for each character ;) */
				user->WriteServ("NOTICE "+std::string(user->nick)+" :*** Invalid characters in channel name or name too long");
				return CMD_FAILURE;
			}

			/* For local users, we send the JoinUser which may create a channel and set its TS.
			 * For non-local users, we just return CMD_SUCCESS, knowing this will propagate it where it needs to be
			 * and then that server will generate the users JOIN or FJOIN instead.
			 */
			if (IS_LOCAL(dest))
			{
				Channel::JoinUser(ServerInstance, dest, parameters[1].c_str(), true, "", false, ServerInstance->Time());
				/* Fix for dotslasher and w00t - if the join didnt succeed, return CMD_FAILURE so that it doesnt propagate */
				Channel* n = ServerInstance->FindChan(parameters[1]);
				if (n)
				{
					if (n->HasUser(dest))
					{
						ServerInstance->SNO->WriteToSnoMask('A', std::string(user->nick)+" used SAJOIN to make "+std::string(dest->nick)+" join "+parameters[1]);
						return CMD_SUCCESS;
					}
					else
					{
						user->WriteServ("NOTICE "+std::string(user->nick)+" :*** Could not join "+std::string(dest->nick)+" to "+parameters[1]+" (User is probably banned, or blocking modes)");
						return CMD_FAILURE;
					}
				}
				else
				{
					user->WriteServ("NOTICE "+std::string(user->nick)+" :*** Could not join "+std::string(dest->nick)+" to "+parameters[1]);
					return CMD_FAILURE;
				}
			}
			else
			{
				ServerInstance->SNO->WriteToSnoMask('A', std::string(user->nick)+" sent remote SAJOIN to make "+std::string(dest->nick)+" join "+parameters[1]);
				return CMD_SUCCESS;
			}
		}
		else
		{
			user->WriteServ("NOTICE "+std::string(user->nick)+" :*** No such nickname "+parameters[0]);
			return CMD_FAILURE;
		}
	}
};

class ModuleSajoin : public Module
{
	CommandSajoin*	mycommand;
 public:
	ModuleSajoin(InspIRCd* Me)
		: Module(Me)
	{

		mycommand = new CommandSajoin(ServerInstance);
		ServerInstance->AddCommand(mycommand);

	}

	virtual ~ModuleSajoin()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}

};

MODULE_INIT(ModuleSajoin)
