/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

/* $ModDesc: Provides an extended version of /WHO for opers */

#include "inspircd_config.h"
#include "inspircd.h"
#include "inspircd_io.h"
#include <time.h>
#include <string>
#ifdef GCC3
#include <ext/hash_map>
#else
#include <hash_map>
#endif
#include <map>
#include <sstream>
#include <vector>
#include <deque>
#include "users.h"
#include "globals.h"
#include "modules.h"
#include "helperfuncs.h"
#include "message.h"
#include "hashcomp.h"
#include "typedefs.h"

extern ServerConfig* Config;
extern InspIRCd* ServerInstance;
extern user_hash clientlist;
extern chan_hash chanlist;
extern std::vector<userrec*> all_opers;
extern std::vector<userrec*> local_users;

class ModuleOperWho : public Module
{
	Server* Srv;
 public:
	ModuleOperWho(Server* Me) : Module::Module(Me)
	{
		Srv = Me;
	}

	virtual void Implements(char* List)
	{
		List[I_OnPreCommand] = 1;
	}

	virtual int OnPreCommand(const std::string &command, char **parameters, int pcnt, userrec *user, bool validated)
	{

		if ((!*user->oper) || (command != "WHO"))
			return 0;

		chanrec* Ptr = NULL;
		char tmp[10];
	
		if (!pcnt)
		{
			for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
			{
				*tmp = 0;
				Ptr = ((ucrec*)*(i->second->chans.begin()))->channel;
				if (*i->second->awaymsg) {
					strlcat(tmp, "G", 9);
				} else {
					strlcat(tmp, "H", 9);
				}
				if (*i->second->oper) { strlcat(tmp, "*", 9); }
				WriteServ(user->fd,"352 %s %s %s %s %s %s %s :0 %s",user->nick, Ptr ? Ptr->name : "*", i->second->ident, i->second->host, i->second->server, i->second->nick, tmp, i->second->fullname);
			}
			WriteServ(user->fd,"315 %s * :End of /WHO list.",user->nick);
			return 1;
		}
		if (pcnt == 1)
		{
			if ((!strcmp(parameters[0],"0")) || (!strcmp(parameters[0],"*")))
			{
				if ((user->chans.size()) && (((ucrec*)*(user->chans.begin()))->channel))
				{
				  	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
					{
						Ptr = ((ucrec*)*(i->second->chans.begin()))->channel;
						// suggested by phidjit and FCS
						if ((!common_channels(user,i->second)) && (isnick(i->second->nick)))
						{
							// Bug Fix #29
							*tmp = 0;
							if (*i->second->awaymsg) {
								strlcat(tmp, "G", 9);
							} else {
								strlcat(tmp, "H", 9);
							}
							if (*i->second->oper) { strlcat(tmp, "*", 9); }
							WriteServ(user->fd,"352 %s %s %s %s %s %s %s :0 %s",user->nick, Ptr ? Ptr->name : "*", i->second->ident, i->second->host, i->second->server, i->second->nick, tmp, i->second->fullname);
						}
					}
				}
				if (Ptr)
				{
					WriteServ(user->fd,"315 %s %s :End of /WHO list.",user->nick , parameters[0]);
				}
				else
				{
					WriteServ(user->fd,"315 %s %s :End of /WHO list.",user->nick, parameters[0]);
				}
				return 1;
			}
			if (parameters[0][0] == '#')
			{
				Ptr = FindChan(parameters[0]);
				if (Ptr)
				{
				  	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
					{
						if ((Ptr->HasUser(i->second)) && (isnick(i->second->nick)))
						{
							// Fix Bug #29 - Part 2..
							*tmp = 0;
							if (*i->second->awaymsg) {
								strlcat(tmp, "G", 9);
							} else {
								strlcat(tmp, "H", 9);
							}
							if (*i->second->oper) { strlcat(tmp, "*", 9); }
							strlcat(tmp, cmode(i->second, Ptr),5);
							WriteServ(user->fd,"352 %s %s %s %s %s %s %s :0 %s",user->nick, Ptr->name, i->second->ident, i->second->host, i->second->server, i->second->nick, tmp, i->second->fullname);
	
						}
					}
					WriteServ(user->fd,"315 %s %s :End of /WHO list.",user->nick, parameters[0]);
				}
				else
				{
					WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
				}
				return 1;
			}
			else
			{
				userrec* u = Find(parameters[0]);
				if (u)
				{
					// Bug Fix #29 -- Part 29..
					*tmp = 0;
					if (*u->awaymsg) {
						strlcat(tmp, "G" ,9);
					} else {
						strlcat(tmp, "H" ,9);
					}
					if (*u->oper) { strlcat(tmp, "*" ,9); }
					WriteServ(user->fd,"352 %s %s %s %s %s %s %s :0 %s",user->nick, u->chans.size() && ((ucrec*)*(u->chans.begin()))->channel ? ((ucrec*)*(u->chans.begin()))->channel->name
					: "*", u->ident, u->dhost, u->server, u->nick, tmp, u->fullname);
				}
				WriteServ(user->fd,"315 %s %s :End of /WHO list.",user->nick, parameters[0]);
				return 1;
			}
		}
		if (pcnt == 2)
		{
			if ((!strcmp(parameters[0],"0")) || (!strcmp(parameters[0],"*")) && (!strcmp(parameters[1],"o")))
			{
			  	for (std::vector<userrec*>::iterator i = all_opers.begin(); i != all_opers.end(); i++)
				{
					// If i were a rich man.. I wouldn't need to me making these bugfixes..
					// But i'm a poor bastard with nothing better to do.
					userrec* oper = *i;
					*tmp = 0;
					if (*oper->awaymsg) {
						strlcat(tmp, "G" ,9);
					} else {
						strlcat(tmp, "H" ,9);
					}
					WriteServ(user->fd,"352 %s %s %s %s %s %s %s* :0 %s", user->nick, oper->chans.size() && ((ucrec*)*(oper->chans.begin()))->channel ?
					((ucrec*)*(oper->chans.begin()))->channel->name : "*", oper->ident, oper->host, oper->server, oper->nick, tmp, oper->fullname);
				}
				WriteServ(user->fd,"315 %s %s :End of /WHO list.",user->nick, parameters[0]);
				return 1;
			}
		}
		return 0;
	}
	
	virtual ~ModuleOperWho()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_VENDOR);
	}
};


class ModuleOperWhoFactory : public ModuleFactory
{
 public:
	ModuleOperWhoFactory()
	{
	}
	
	~ModuleOperWhoFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleOperWho(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleOperWhoFactory;
}
