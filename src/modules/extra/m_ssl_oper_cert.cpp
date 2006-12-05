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
/* $ModDep: ssl_cert.h */

using namespace std;

#include <stdio.h>
#include "inspircd_config.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"
#include "ssl_cert.h"
#include "wildcard.h"

/** Handle /FINGERPRINT
 */
class cmd_fingerprint : public command_t
{
 public:
	cmd_fingerprint (InspIRCd* Instance) : command_t(Instance,"FINGERPRINT", 0, 1)
	{
		this->source = "m_ssl_oper_cert.so";
		syntax = "<nickname>";
	}       
	          
	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		userrec* target = ServerInstance->FindNick(parameters[0]);
		if (target)
		{
			ssl_cert* cert;
			if (target->GetExt("ssl_cert",cert))
			{
				if (cert->GetFingerprint().length())
				{
					user->WriteServ("NOTICE %s :Certificate fingerprint for %s is %s",user->nick,target->nick,cert->GetFingerprint().c_str());
					return CMD_SUCCESS;
				}
				else
				{
					user->WriteServ("NOTICE %s :Certificate fingerprint for %s does not exist!", user->nick,target->nick);
					return CMD_FAILURE;
				}
			}
			else
			{
				user->WriteServ("NOTICE %s :Certificate fingerprint for %s does not exist!", user->nick, target->nick);
				return CMD_FAILURE;
			}
		}
		else
		{
			user->WriteServ("401 %s %s :No such nickname", user->nick, parameters[0]);
			return CMD_FAILURE;
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
		List[I_OnPreCommand] = 1;
	}


	bool OneOfMatches(const char* host, const char* ip, const char* hostlist)
	{
		std::stringstream hl(hostlist);
		std::string xhost;
		while (hl >> xhost)
		{
			if (match(host,xhost.c_str()) || match(ip,xhost.c_str(),true))
			{
				return true;
			}
		}
		return false;
	}


	virtual int OnPreCommand(const std::string &command, const char** parameters, int pcnt, userrec *user, bool validated, const std::string &original_line)
	{
		irc::string cmd = command.c_str();
		
		if ((cmd == "OPER") && (validated))
		{
			char LoginName[MAXBUF];
			char Password[MAXBUF];
			char OperType[MAXBUF];
			char HostName[MAXBUF];
			char TheHost[MAXBUF];
			char TheIP[MAXBUF];
			char FingerPrint[MAXBUF];

			snprintf(TheHost,MAXBUF,"%s@%s",user->ident,user->host);
			snprintf(TheIP, MAXBUF,"%s@%s",user->ident,user->GetIPString());

			HasCert = user->GetExt("ssl_cert",cert);
			ServerInstance->Log(DEBUG,"HasCert=%d",HasCert);
			for (int i = 0; i < ServerInstance->Config->ConfValueEnum(ServerInstance->Config->config_data, "oper"); i++)
			{
				ServerInstance->Config->ConfValue(ServerInstance->Config->config_data, "oper", "name", i, LoginName, MAXBUF);
				ServerInstance->Config->ConfValue(ServerInstance->Config->config_data, "oper", "password", i, Password, MAXBUF);
				ServerInstance->Config->ConfValue(ServerInstance->Config->config_data, "oper", "type", i, OperType, MAXBUF);
				ServerInstance->Config->ConfValue(ServerInstance->Config->config_data, "oper", "host", i, HostName, MAXBUF);
				ServerInstance->Config->ConfValue(ServerInstance->Config->config_data, "oper", "fingerprint",  i, FingerPrint, MAXBUF);
				
				if (*FingerPrint)
				{
					if ((!strcmp(LoginName,parameters[0])) && (!ServerInstance->OperPassCompare(Password,parameters[1], i)) && (OneOfMatches(TheHost,TheIP,HostName)))
					{
						/* This oper would match */
						if ((!cert) || (cert->GetFingerprint() != FingerPrint))
						{
							user->WriteServ("491 %s :This oper login name requires a matching key fingerprint.",user->nick);
							ServerInstance->SNO->WriteToSnoMask('o',"'%s' cannot oper, does not match fingerprint", user->nick);
							ServerInstance->Log(DEFAULT,"OPER: Failed oper attempt by %s!%s@%s: credentials valid, but wrong fingerprint.",user->nick,user->ident,user->host);
							return 1;
						}
					}
				}
			}
		}
		return 0;
	}

	virtual Version GetVersion()
	{
		return Version(1,1,0,0,VF_VENDOR,API_VERSION);
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
