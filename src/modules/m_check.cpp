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

		checkstr = "304 " + std::string(user->nick) + " :CHECK";

		targuser = Srv->FindNick(std::string(parameters[0]));
		targchan = Srv->FindChannel(std::string(parameters[0]));

		/*
		 * Syntax of a /check reply:
		 *  :server.name 304 target :CHECK START <target>
		 *  :server.name 304 target :CHECK <field> <value>
		 *  :server.name 304 target :CHECK END
		 */

		Srv->SendTo(NULL, user, checkstr + " START " + std::string(parameters[0]));

		if (targuser)
		{
			/* /check on a user */
			Srv->SendTo(NULL, user, checkstr + " nuh " + std::string(targuser->GetFullHost()));
			Srv->SendTo(NULL, user, checkstr + " realnuh " + std::string(targuser->GetFullRealHost()));
			Srv->SendTo(NULL, user, checkstr + " realname " + std::string(targuser->fullname));
			Srv->SendTo(NULL, user, checkstr + " modes +" + std::string(targuser->modes));
			Srv->SendTo(NULL, user, checkstr + " server " + std::string(targuser->server));
			if (targuser->awaymsg[0] != 0)
			{
				/* user is away */
				Srv->SendTo(NULL, user, checkstr + " awaymsg " + std::string(targuser->awaymsg));
			}
			if (targuser->oper[0] != 0)
			{
				/* user is an oper of type ____ */
				Srv->SendTo(NULL, user, checkstr + " opertype " + std::string(targuser->oper));
			}
			if (IS_LOCAL(targuser))
			{
				/* port information is only held for a local user! */
				Srv->SendTo(NULL, user, checkstr + " onport " + ConvToStr(targuser->port));
			}

			chliststr = chlist(targuser, targuser);
			if (chliststr.length())
			{
				if (chliststr.length() > 400)
				{
					/* XXX - this sucks. deal with it. */
					std::stringstream chstream(chliststr);
					std::string line = "";
					std::string cname = "";
					while (!chstream.eof())
					{
						chstream >> cname;
						line = line + cname + " ";
						if (line.length() > 400)
						{
							Srv->SendTo(NULL, user, checkstr + " onchans " + line);
							line = "";
						}
					}
					if (line.length())
					{
						Srv->SendTo(NULL, user, checkstr + " onchans " + line);
					}
				}
				else
				{
					Srv->SendTo(NULL, user, checkstr + " onchans " + chliststr);
				}				
			}
		}
		else if (targchan)
		{
			/* /check on a channel */
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

