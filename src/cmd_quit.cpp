/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd_config.h"
#include "inspircd.h"
#include "configreader.h"
#include <string>
#include <vector>
#include "users.h"
#include "ctables.h"
#include "globals.h"
#include "modules.h"
#include "dynamic.h"
#include "wildcard.h"
#include "message.h"
#include "commands.h"
#include "mode.h"
#include "xline.h"
#include "inspstring.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "socketengine.h"
#include "commands/cmd_quit.h"

extern InspIRCd* ServerInstance;
extern int MODCOUNT;
extern ModuleList modules;
extern FactoryList factory;
extern time_t TIME;

void cmd_quit::Handle (const char** parameters, int pcnt, userrec *user)
{
	user_hash::iterator iter = ServerInstance->clientlist.find(user->nick);
	char reason[MAXBUF];

	if (user->registered == REG_ALL)
	{
		/* theres more to do here, but for now just close the socket */
		if (pcnt == 1)
		{
			if (*parameters[0] == ':')
				parameters[0]++;

			strlcpy(reason, parameters[0],MAXQUIT-1);

			/* We should only prefix the quit for a local user. Remote users have
			 * already been prefixed, where neccessary, by the upstream server.
			 */
			if (user->fd > -1)
			{
				user->Write("ERROR :Closing link (%s@%s) [%s%s]",user->ident,user->host,ServerInstance->Config->PrefixQuit,parameters[0]);
				ServerInstance->WriteOpers("*** Client exiting: %s!%s@%s [%s%s]",user->nick,user->ident,user->host,ServerInstance->Config->PrefixQuit,parameters[0]);
				user->WriteCommonExcept("QUIT :%s%s",ServerInstance->Config->PrefixQuit,parameters[0]);
			}
			else
			{
				ServerInstance->WriteOpers("*** Client exiting at %s: %s!%s@%s [%s]",user->server,user->nick,user->ident,user->host,parameters[0]);
				user->WriteCommonExcept("QUIT :%s",parameters[0]);
			}
			FOREACH_MOD(I_OnUserQuit,OnUserQuit(user,std::string(ServerInstance->Config->PrefixQuit)+std::string(parameters[0])));

		}
		else
		{
			user->Write("ERROR :Closing link (%s@%s) [QUIT]",user->ident,user->host);
			ServerInstance->WriteOpers("*** Client exiting: %s!%s@%s [Client exited]",user->nick,user->ident,user->host);
			user->WriteCommonExcept("QUIT :Client exited");
			FOREACH_MOD(I_OnUserQuit,OnUserQuit(user,"Client exited"));

		}
		user->AddToWhoWas();
	}

	FOREACH_MOD(I_OnUserDisconnect,OnUserDisconnect(user));

	/* push the socket on a stack of sockets due to be closed at the next opportunity */
	if (user->fd > -1)
	{
		ServerInstance->SE->DelFd(user->fd);
		if (find(ServerInstance->local_users.begin(),ServerInstance->local_users.end(),user) != ServerInstance->local_users.end())
		{
			log(DEBUG,"Delete local user");
			ServerInstance->local_users.erase(find(ServerInstance->local_users.begin(),ServerInstance->local_users.end(),user));
		}
		user->CloseSocket();
	}
	
	if (iter != ServerInstance->clientlist.end())
	{
		ServerInstance->clientlist.erase(iter);
	}

	if (user->registered == REG_ALL) {
		user->PurgeEmptyChannels();
	}
	if (user->fd > -1)
		ServerInstance->fd_ref_table[user->fd] = NULL;
	DELETE(user);
}
