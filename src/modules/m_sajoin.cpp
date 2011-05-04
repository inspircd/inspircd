/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
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
	CommandSajoin(Module* Creator) : Command(Creator,"SAJOIN", 2)
	{
		flags_needed = 'o'; Penalty = 0; syntax = "<nick> <channel>";
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
				user->WriteServ("NOTICE %s :*** Invalid characters in channel name or name too long", user->nick.c_str());
				return CMD_FAILURE;
			}

			/* For local users, we send the JoinUser which may create a channel and set its TS.
			 * For non-local users, we just return CMD_SUCCESS, knowing this will propagate it where it needs to be
			 * and then that server will generate the users JOIN or FJOIN instead.
			 */
			if (IS_LOCAL(dest))
			{
				Channel::JoinUser(dest, parameters[1].c_str(), true, "", false, ServerInstance->Time());
				/* Fix for dotslasher and w00t - if the join didnt succeed, return CMD_FAILURE so that it doesnt propagate */
				Channel* n = ServerInstance->FindChan(parameters[1]);
				if (n)
				{
					if (n->HasUser(dest))
					{
						ServerInstance->SNO->WriteToSnoMask('a', std::string(user->nick)+" used SAJOIN to make "+std::string(dest->nick)+" join "+parameters[1]);
						return CMD_SUCCESS;
					}
					else
					{
						user->WriteServ("NOTICE %s :*** Could not join %s to %s (User is probably banned, or blocking modes)", user->nick.c_str(), dest->nick.c_str(), parameters[1].c_str());
						return CMD_FAILURE;
					}
				}
				else
				{
					user->WriteServ("NOTICE %s :*** Could not join %s to %s", user->nick.c_str(), dest->nick.c_str(), parameters[1].c_str());
					return CMD_FAILURE;
				}
			}
			else
			{
				ServerInstance->SNO->WriteToSnoMask('a', std::string(user->nick)+" sent remote SAJOIN to make "+std::string(dest->nick)+" join "+parameters[1]);
				return CMD_SUCCESS;
			}
		}
		else
		{
			user->WriteServ("NOTICE %s :*** No such nickname %s", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);
		if (dest)
			return ROUTE_OPT_UCAST(dest->server);
		return ROUTE_LOCALONLY;
	}
};

class ModuleSajoin : public Module
{
	CommandSajoin cmd;
 public:
	ModuleSajoin()
		: cmd(this) {}
	void init()
	{
		ServerInstance->AddCommand(&cmd);
	}

	virtual ~ModuleSajoin()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for unreal-style SAJOIN command", VF_OPTCOMMON | VF_VENDOR);
	}

};

MODULE_INIT(ModuleSajoin)
