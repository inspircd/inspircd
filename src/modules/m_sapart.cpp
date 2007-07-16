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

/* $ModDesc: Provides support for unreal-style SAPART command */

/** Handle /SAPART
 */
class cmd_sapart : public command_t
{
 public:
	cmd_sapart (InspIRCd* Instance) : command_t(Instance,"SAPART", 'o', 2)
	{
		this->source = "m_sapart.so";
		syntax = "<nick> <channel>";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		userrec* dest = ServerInstance->FindNick(parameters[0]);
		chanrec* channel = ServerInstance->FindChan(parameters[1]);
		if (dest && channel)
		{
			if (ServerInstance->ULine(dest->server))
			{
				user->WriteServ("990 %s :Cannot use an SA command on a u-lined client",user->nick);
				return CMD_FAILURE;
			}

			/* For local clients, directly part them generating a PART message. For remote clients,
			 * just return CMD_SUCCESS knowing the protocol module will route the SAPART to the users
			 * local server and that will generate the PART instead
			 */
			if (IS_LOCAL(dest))
			{
				if (!channel->PartUser(dest, dest->nick))
					delete channel;
				chanrec* n = ServerInstance->FindChan(parameters[1]);
				if (!n)
				{
					ServerInstance->WriteOpers("*** "+std::string(user->nick)+" used SAPART to make "+dest->nick+" part "+parameters[1]);
					return CMD_SUCCESS;
				}
				else
				{
					if (!n->HasUser(dest))
					{
						ServerInstance->WriteOpers("*** "+std::string(user->nick)+" used SAPART to make "+dest->nick+" part "+parameters[1]);
						return CMD_SUCCESS;
					}
					else
					{
						user->WriteServ("NOTICE %s :*** Unable to make %s part %s",user->nick, dest->nick, parameters[1]);
						return CMD_FAILURE;
					}
				}
			}
			else
			{
				ServerInstance->WriteOpers("*** "+std::string(user->nick)+" sent remote SAPART to make "+dest->nick+" part "+parameters[1]);
			}

			return CMD_SUCCESS;
		}
		else
		{
			user->WriteServ("NOTICE %s :*** Invalid nickname or channel", user->nick);
		}

		return CMD_FAILURE;
	}
};


class ModuleSapart : public Module
{
	cmd_sapart*	mycommand;
 public:
	ModuleSapart(InspIRCd* Me)
		: Module(Me)
	{
		
		mycommand = new cmd_sapart(ServerInstance);
		ServerInstance->AddCommand(mycommand);
	}
	
	virtual ~ModuleSapart()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
	
};

MODULE_INIT(ModuleSapart)

