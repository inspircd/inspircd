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

/* $ModDesc: Allows for MD5 encrypted oper passwords */

using namespace std;

#include <stdio.h>
#include "inspircd_config.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"
#include "ssl_cert.h"

class cmd_fingerprint : public command_t
{
 public:
	cmd_fingerprint (InspIRCd* Instance) : command_t(Instance,"FINGERPRINT", 0, 1)
	{
		this->source = "m_ssl_oper_cert.so";
		syntax = "<nickname>";
	}       
	          
	void Handle (const char** parameters, int pcnt, userrec *user)
	{
		userrec* target = ServerInstance->FindNick(parameters[0]);
		if (target)
		{
			ssl_cert* cert;
			if (target->GetExt("ssl_cert",cert))
			{
				if (cert->GetFingerprint().length())
					user->WriteServ("NOTICE %s :Certificate fingerprint for %s is %s",user->nick,target->nick,cert->GetFingerprint().c_str());
				else
					user->WriteServ("NOTICE %s :Certificate fingerprint for %s does not exist!", user->nick,target->nick);
			}
			else
			{
				user->WriteServ("NOTICE %s :Certificate fingerprint for %s does not exist!", user->nick, target->nick);
			}
		}
		else
		{
			user->WriteServ("401 %s %s :No such nickname", user->nick, parameters[0]);
		}
	}
};


class ModuleOperSSLCert : public Module
{
	ssl_cert* cert;
	bool HasCert;
	cmd_fingerprint* mycommand;
 public:

	ModuleOperSSLCert(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		mycommand = new cmd_fingerprint(ServerInstance);
		ServerInstance->AddCommand(mycommand);
	}
	
	virtual ~ModuleOperSSLCert()
	{
	}

	void Implements(char* List)
	{
		List[I_OnOperCompare] = List[I_OnPreCommand] = 1;
	}

	virtual int OnOperCompare(const std::string &data, const std::string &input)
	{
		ServerInstance->Log(DEBUG,"HasCert=%d, data='%s' input='%s'",HasCert,data.c_str(), input.c_str());
		if (((data.length()) && (data.length() == cert->GetFingerprint().length())))
		{
			ServerInstance->Log(DEBUG,"Lengths match, cert='%s'",cert->GetFingerprint().c_str());
			if (data == cert->GetFingerprint())
			{
				ServerInstance->Log(DEBUG,"Return 1");
				return 1;
			}
			else
			{
				ServerInstance->Log(DEBUG,"'%s' != '%s'",data.c_str(), cert->GetFingerprint().c_str());
				return 0;
			}
		}
		else
		{
			ServerInstance->Log(DEBUG,"Lengths dont match");
			return 0;
		}
	}

	virtual int OnPreCommand(const std::string &command, const char** parameters, int pcnt, userrec *user, bool validated)
	{
		irc::string cmd = command.c_str();
		
		if ((cmd == "OPER") && (validated == 1))
		{
			HasCert = user->GetExt("ssl_cert",cert);
			ServerInstance->Log(DEBUG,"HasCert=%d",HasCert);
		}
		return 0;
	}

	virtual Version GetVersion()
	{
		return Version(1,1,0,0,VF_VENDOR);
	}
};

class ModuleOperSSLCertFactory : public ModuleFactory
{
 public:
	ModuleOperSSLCertFactory()
	{
	}
	
	~ModuleOperSSLCertFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleOperSSLCert(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleOperSSLCertFactory;
}
