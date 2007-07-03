/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides support for unreal-style SAJOIN command */

/** Handle /SAJOIN
 */
class cmd_sajoin : public command_t
{
 public:
 cmd_sajoin (InspIRCd* Instance) : command_t(Instance,"SAJOIN", 'o', 2)
	{
		this->source = "m_sajoin.so";
		syntax = "<nick> <channel>";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		userrec* dest = ServerInstance->FindNick(parameters[0]);
		if (dest)
		{
			if (ServerInstance->ULine(dest->server))
			{
				user->WriteServ("990 %s :Cannot use an SA command on a u-lined client",user->nick);
				return CMD_FAILURE;
			}
			if (!ServerInstance->IsChannel(parameters[1]))
			{
				/* we didn't need to check this for each character ;) */
				user->WriteServ("NOTICE "+std::string(user->nick)+" :*** Invalid characters in channel name");
				return CMD_FAILURE;
			}

			/* For local users, we send the JoinUser which may create a channel and set its TS.
			 * For non-local users, we just return CMD_SUCCESS, knowing this will propogate it where it needs to be
			 * and then that server will generate the users JOIN or FJOIN instead.
			 */
			if (IS_LOCAL(dest))
			{
				chanrec::JoinUser(ServerInstance, dest, parameters[1], true, "", ServerInstance->Time(true));
				/* Fix for dotslasher and w00t - if the join didnt succeed, return CMD_FAILURE so that it doesnt propogate */
				chanrec* n = ServerInstance->FindChan(parameters[1]);
				if (n)
				{
					if (n->HasUser(dest))
					{
						ServerInstance->WriteOpers("*** "+std::string(user->nick)+" used SAJOIN to make "+std::string(dest->nick)+" join "+parameters[1]);
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
				ServerInstance->WriteOpers("*** "+std::string(user->nick)+" sent remote SAJOIN to make "+std::string(dest->nick)+" join "+parameters[1]);
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
	cmd_sajoin*	mycommand;
 public:
	ModuleSajoin(InspIRCd* Me)
		: Module(Me)
	{
		
		mycommand = new cmd_sajoin(ServerInstance);
		ServerInstance->AddCommand(mycommand);
	}
	
	virtual ~ModuleSajoin()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
	
};

MODULE_INIT(ModuleSajoin)
