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
#include "message.h"
#include "commands.h"
#include "inspircd.h"
#include "helperfuncs.h"

/* $ModDesc: Provides the /check command to retrieve information on a user, channel, or IP address */

static Server *Srv;

class cmd_check : public command_t
{
 public:
	cmd_check() : command_t("CHECK", 'o', 1)
	{
		this->source = "m_check.so";
	}

	void Handle (char **parameters, int pcnt, userrec *user)
	{
		userrec *targuser;
		chanrec *targchan;
		std::string checkstr;
		std::string chliststr;

		char timebuf[60];
		struct tm *mytime;


		checkstr = "304 " + std::string(user->nick) + " :CHECK";

		targuser = Srv->FindNick(std::string(parameters[0]));
		targchan = Srv->FindChannel(std::string(parameters[0]));

		/*
		 * Syntax of a /check reply:
		 *  :server.name 304 target :CHECK START <target>
		 *  :server.name 304 target :CHECK <field> <value>
		 *  :server.name 304 target :CHECK END
		 */

		Srv->SendTo(NULL, user, checkstr + " START " + parameters[0]);

		if (targuser)
		{
			/* /check on a user */
			Srv->SendTo(NULL, user, checkstr + " nuh " + targuser->GetFullHost());
			Srv->SendTo(NULL, user, checkstr + " realnuh " + targuser->GetFullRealHost());
			Srv->SendTo(NULL, user, checkstr + " realname " + targuser->fullname);
			Srv->SendTo(NULL, user, checkstr + " modes +" + targuser->modes);
			Srv->SendTo(NULL, user, checkstr + " server " + targuser->server);
			if (targuser->awaymsg[0] != 0)
			{
				/* user is away */
				Srv->SendTo(NULL, user, checkstr + " awaymsg " + targuser->awaymsg);
			}
			if (targuser->oper[0] != 0)
			{
				/* user is an oper of type ____ */
				Srv->SendTo(NULL, user, checkstr + " opertype " + targuser->oper);
			}
			if (IS_LOCAL(targuser))
			{
				/* port information is only held for a local user! */
				Srv->SendTo(NULL, user, checkstr + " onport " + ConvToStr(targuser->port));
			}

			chliststr = chlist(targuser, targuser);
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
			Srv->SendTo(NULL, user, checkstr + " created " + timebuf);

			if (targchan->topic[0] != 0)
			{
				/* there is a topic, assume topic related information exists */
				Srv->SendTo(NULL, user, checkstr + " topic " + targchan->topic);
				Srv->SendTo(NULL, user, checkstr + " topic_setby " + targchan->setby);
				mytime = gmtime(&topic_time);
				strftime(timebuf, 59, "%Y/%m/%d - %H:%M:%S", mytime);
				Srv->SendTo(NULL, user, checkstr + " topic_setat " + timebuf);
			}

			Srv->SendTo(NULL, user, checkstr + " modes " + chanmodes(targchan, true));
			Srv->SendTo(NULL, user, checkstr + " membercount " + ConvToStr(targchan->GetUserCounter()));
			
			/* now the ugly bit, spool current members of a channel. :| */

			CUList *ulist= targchan->GetUsers();

			/* note that unlike /names, we do NOT check +i vs in the channel */
			for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
			{
				char list[MAXBUF];
				char tmpbuf[MAXBUF];
				char* ptr = list;
				int flags = cflags(i->second, targchan);
				/*
				 * find how many connections from this user's IP -- unlike Asuka,
				 * I define a clone as coming from the same host. --w00t
				 */
				sprintf(ptr, "%lu    ", FindMatchingGlobal(i->second));
				
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
				
				Srv->SendTo(NULL, user, checkstr + " member " + ptr);
			}
		}
		else
		{
			/*  /check on an IP address, or something that doesn't exist */
		}

		Srv->SendTo(NULL, user, checkstr + " END " + std::string(parameters[0]));
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

