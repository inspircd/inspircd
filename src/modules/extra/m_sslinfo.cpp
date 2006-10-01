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

#include <stdio.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "ssl_cert.h"
#include "wildcard.h"
#include "inspircd.h"
#include "dns.h"

/* $ModDesc: Provides /sslinfo command used to test who a mask matches */
	 
class cmd_sslinfo : public command_t
{
 public:
	cmd_sslinfo (InspIRCd* Instance) : command_t(Instance,"SSLINFO", 0, 1)
	{
		this->source = "m_sslinfo.so";
		this->syntax = "<nick>";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		userrec* target = ServerInstance->FindNick(parameters[0]);
		ssl_cert* cert;

		if (target)
		{
			if (target->GetExt("ssl_cert", cert))
			{
				if (cert->GetError().length())
				{
					user->WriteServ("NOTICE %s :*** Error:             %s", user->nick, cert->GetError().c_str());
				}
				user->WriteServ("NOTICE %s :*** Distinguised Name: %s", user->nick, cert->GetDN().c_str());
				user->WriteServ("NOTICE %s :*** Issuer:            %s", user->nick, cert->GetIssuer().c_str());
				user->WriteServ("NOTICE %s :*** Key Fingerprint:   %s", user->nick, cert->GetFingerprint().c_str());
				return CMD_SUCCESS;
			}
			else
			{
				user->WriteServ("NOTICE %s :*** No SSL certificate information for this user.", user->nick);
				return CMD_FAILURE;
			}
		}
		else
			user->WriteServ("401 %s %s :No such nickname", user->nick, parameters[0]);

		return CMD_FAILURE;
	}
};

class ModuleSSLInfo : public Module
{
	cmd_sslinfo* newcommand;
 public:
	ModuleSSLInfo(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		newcommand = new cmd_sslinfo(ServerInstance);
		ServerInstance->AddCommand(newcommand);
	}

	void Implements(char* List)
	{
	}

	virtual ~ModuleSSLInfo()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_VENDOR, API_VERSION);
	}
};


class ModuleSSLInfoFactory : public ModuleFactory
{
 public:
	ModuleSSLInfoFactory()
	{
	}
	
	~ModuleSSLInfoFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleSSLInfo(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSSLInfoFactory;
}

