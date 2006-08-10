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

#include "users.h"
#include "channels.h"
#include "modules.h"
#include "commands.h"
#include "inspircd.h"
#include "helperfuncs.h"
#include "wildcard.h"

/* $ModDesc: Provides the /check command to retrieve information on a user, channel, or IP address */

extern InspIRCd* ServerInstance;

static Server *Srv;

class cmd_check : public command_t
{
 public:
	cmd_check() : command_t("CHECK", 'o', 1)
	{
		this->source = "m_check.so";
		syntax = "<nickname>|<ip>|<hostmask>|<channel>";
	}

	void Handle (const char** parameters, int pcnt, userrec *user)
	{
		userrec *targuser;
		chanrec *targchan;
		std::string checkstr;
		std::string chliststr;

		char timebuf[60];
		struct tm *mytime;


		checkstr = "304 " + std::string(user->nick) + " :CHECK";

		targuser = ServerInstance->FindNick(parameters[0]);
		targchan = ServerInstance->FindChan(parameters[0]);

		/*
		 * Syntax of a /check reply:
		 *  :server.name 304 target :CHECK START <target>
		 *  :server.name 304 target :CHECK <field> <value>
		 *  :server.name 304 target :CHECK END
		 */

		user->WriteServ(checkstr + " START " + parameters[0]);

		if (targuser)
		{
			/* /check on a user */
			user->WriteServ(checkstr + " nuh " + targuser->GetFullHost());
			user->WriteServ(checkstr + " realnuh " + targuser->GetFullRealHost());
			user->WriteServ(checkstr + " realname " + targuser->fullname);
			user->WriteServ(checkstr + " modes +" + targuser->FormatModes());
			user->WriteServ(checkstr + " server " + targuser->server);
			if (targuser->awaymsg[0] != 0)
			{
				/* user is away */
				user->WriteServ(checkstr + " awaymsg " + targuser->awaymsg);
			}
			if (targuser->oper[0] != 0)
			{
				/* user is an oper of type ____ */
				user->WriteServ(checkstr + " opertype " + targuser->oper);
			}
			if (IS_LOCAL(targuser))
			{
				/* port information is only held for a local user! */
				user->WriteServ(checkstr + " onport " + ConvToStr(targuser->GetPort()));
			}

			chliststr = targuser->ChannelList(targuser);
			std::stringstream dump(chliststr);

			Srv->DumpText(user,checkstr + " onchans ", dump);
		}
		else if (targchan)
		{
			/* /check on a channel */
			time_t creation_time = targchan->created;
			time_t topic_time = targchan->topicset;

			mytime = gmtime(&creation_time);
			strftime(timebuf, 59, "%Y/%m/%d - %H:%M:%S", mytime);
			user->WriteServ(checkstr + " created " + timebuf);

			if (targchan->topic[0] != 0)
			{
				/* there is a topic, assume topic related information exists */
				user->WriteServ(checkstr + " topic " + targchan->topic);
				user->WriteServ(checkstr + " topic_setby " + targchan->setby);
				mytime = gmtime(&topic_time);
				strftime(timebuf, 59, "%Y/%m/%d - %H:%M:%S", mytime);
				user->WriteServ(checkstr + " topic_setat " + timebuf);
			}

			user->WriteServ(checkstr + " modes " + targchan->ChanModes(true));
			user->WriteServ(checkstr + " membercount " + ConvToStr(targchan->GetUserCounter()));
			
			/* now the ugly bit, spool current members of a channel. :| */

			CUList *ulist= targchan->GetUsers();

			/* note that unlike /names, we do NOT check +i vs in the channel */
			for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
			{
				char list[MAXBUF];
				char tmpbuf[MAXBUF];
				char* ptr = list;
				int flags = targchan->GetStatusFlags(i->second);
				/*
				 * find how many connections from this user's IP -- unlike Asuka,
				 * I define a clone as coming from the same host. --w00t
				 */
				sprintf(ptr, "%lu    ", i->second->GlobalCloneCount());
				
				if (flags & UCMODE_OP)
				{
					strcat(ptr, "@");
				}
				
				if (flags & UCMODE_HOP)
				{
					strcat(ptr, "%");
				}
				
				if (flags & UCMODE_VOICE)
				{
					strcat(ptr, "+");
				}
				
				sprintf(tmpbuf, "%s (%s@%s) %s ", i->second->nick, i->second->ident, i->second->dhost, i->second->fullname);
				strcat(ptr, tmpbuf);
				
				user->WriteServ(checkstr + " member " + ptr);
			}
		}
		else
		{
			/*  /check on an IP address, or something that doesn't exist */
			long x = 0;

			/* hostname or other */
			for (user_hash::const_iterator a = ServerInstance->clientlist.begin(); a != ServerInstance->clientlist.end(); a++)
			{
				if (match(a->second->host, parameters[0]) || match(a->second->dhost, parameters[0]))
				{
					/* host or vhost matches mask */
					user->WriteServ(checkstr + " match " + ConvToStr(++x) + " " + a->second->GetFullRealHost());
				}
				/* IP address */
				else if (match(a->second->GetIPString(), parameters[0]))
				{
					/* same IP. */
					user->WriteServ(checkstr + " match " + ConvToStr(++x) + " " + a->second->GetFullRealHost());
				}
			}

			user->WriteServ(checkstr + " matches " + ConvToStr(x));
		}

		user->WriteServ(checkstr + " END " + std::string(parameters[0]));
	}
};


class ModuleCheck : public Module
{
 private:
	cmd_check *mycommand;
 public:
	ModuleCheck(Server* Me) : Module::Module(Me)
	{
		Srv = Me;
		mycommand = new cmd_check();
		Srv->AddCommand(mycommand);
	}
	
	virtual ~ModuleCheck()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_VENDOR);
	}

	void Implements(char* List)
	{
		/* we don't hook anything, nothing required */
	}
	
};



class ModuleCheckFactory : public ModuleFactory
{
 public:
	ModuleCheckFactory()
	{
	}
	
	~ModuleCheckFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleCheck(Me);
	}
	
};

extern "C" void * init_module( void )
{
	return new ModuleCheckFactory;
}

