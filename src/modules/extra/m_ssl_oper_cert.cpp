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

/* $ModDesc: Allows for MD5 encrypted oper passwords */
/* $ModDep: transport.h */

#include "inspircd.h"
#include "inspircd_config.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "transport.h"
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
	ConfigReader* cf;
 public:

	ModuleOperSSLCert(InspIRCd* Me)
		: Module(Me)
	{
		mycommand = new cmd_fingerprint(ServerInstance);
		ServerInstance->AddCommand(mycommand);
		cf = new ConfigReader(ServerInstance);
	}

	virtual ~ModuleOperSSLCert()
	{
		delete cf;
	}

	void Implements(char* List)
	{
		List[I_OnPreCommand] = List[I_OnRehash] = 1;
	}

	virtual void OnRehash(userrec* user, const std::string &parameter)
	{
		delete cf;
		cf = new ConfigReader(ServerInstance);
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
			char TheHost[MAXBUF];
			char TheIP[MAXBUF];
			std::string LoginName;
			std::string Password;
			std::string OperType;
			std::string HostName;
			std::string FingerPrint;
			bool SSLOnly;
			char* dummy;

			snprintf(TheHost,MAXBUF,"%s@%s",user->ident,user->host);
			snprintf(TheIP, MAXBUF,"%s@%s",user->ident,user->GetIPString());

			HasCert = user->GetExt("ssl_cert",cert);

			for (int i = 0; i < cf->Enumerate("oper"); i++)
			{
				LoginName = cf->ReadValue("oper", "name", i);
				Password = cf->ReadValue("oper", "password", i);
				OperType = cf->ReadValue("oper", "type", i);
				HostName = cf->ReadValue("oper", "host", i);
				FingerPrint = cf->ReadValue("oper", "fingerprint", i);
				SSLOnly = cf->ReadFlag("oper", "sslonly", i);

				if (SSLOnly || !FingerPrint.empty())
				{
					if ((!strcmp(LoginName.c_str(),parameters[0])) && (!ServerInstance->OperPassCompare(Password.c_str(),parameters[1],i)) && (OneOfMatches(TheHost,TheIP,HostName.c_str())))
					{
						if (SSLOnly && !user->GetExt("ssl", dummy))
						{
							user->WriteServ("491 %s :This oper login name requires an SSL connection.", user->nick);
							return 1;
						}

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

MODULE_INIT(ModuleOperSSLCert);

