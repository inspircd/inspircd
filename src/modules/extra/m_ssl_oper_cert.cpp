/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $ModDesc: Allows for MD5 encrypted oper passwords */
/* $ModDep: transport.h */

#include "inspircd.h"
#include "transport.h"

/** Handle /FINGERPRINT
 */
class CommandFingerprint : public Command
{
 public:
	CommandFingerprint (InspIRCd* Instance) : Command(Instance,"FINGERPRINT", 0, 1)
	{
		this->source = "m_ssl_oper_cert.so";
		syntax = "<nickname>";
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		User* target = ServerInstance->FindNick(parameters[0]);
		if (target)
		{
			ssl_cert* cert;
			if (target->GetExt("ssl_cert",cert))
			{
				if (cert->GetFingerprint().length())
				{
					user->WriteServ("NOTICE %s :Certificate fingerprint for %s is %s",user->nick.c_str(),target->nick.c_str(),cert->GetFingerprint().c_str());
					return CMD_LOCALONLY;
				}
				else
				{
					user->WriteServ("NOTICE %s :Certificate fingerprint for %s does not exist!", user->nick.c_str(),target->nick.c_str());
					return CMD_FAILURE;
				}
			}
			else
			{
				user->WriteServ("NOTICE %s :Certificate fingerprint for %s does not exist!", user->nick.c_str(), target->nick.c_str());
				return CMD_FAILURE;
			}
		}
		else
		{
			user->WriteNumeric(401, "%s %s :No such nickname", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}
	}
};



class ModuleOperSSLCert : public Module
{
	ssl_cert* cert;
	bool HasCert;
	CommandFingerprint* mycommand;
	ConfigReader* cf;
 public:

	ModuleOperSSLCert(InspIRCd* Me)
		: Module(Me)
	{
		mycommand = new CommandFingerprint(ServerInstance);
		ServerInstance->AddCommand(mycommand);
		cf = new ConfigReader(ServerInstance);
		Implementation eventlist[] = { I_OnPreCommand, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual ~ModuleOperSSLCert()
	{
		delete cf;
	}


	virtual void OnRehash(User* user, const std::string &parameter)
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
			if (InspIRCd::Match(host, xhost, ascii_case_insensitive_map) || InspIRCd::MatchCIDR(ip, xhost, ascii_case_insensitive_map))
			{
				return true;
			}
		}
		return false;
	}

	virtual int OnPreCommand(std::string &command, std::vector<std::string> &parameters, User *user, bool validated, const std::string &original_line)
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
			std::string HashType;
			std::string FingerPrint;
			bool SSLOnly;
			char* dummy;

			snprintf(TheHost,MAXBUF,"%s@%s",user->ident.c_str(),user->host.c_str());
			snprintf(TheIP, MAXBUF,"%s@%s",user->ident.c_str(),user->GetIPString());

			HasCert = user->GetExt("ssl_cert",cert);

			for (int i = 0; i < cf->Enumerate("oper"); i++)
			{
				LoginName = cf->ReadValue("oper", "name", i);
				Password = cf->ReadValue("oper", "password", i);
				OperType = cf->ReadValue("oper", "type", i);
				HostName = cf->ReadValue("oper", "host", i);
				HashType = cf->ReadValue("oper", "hash", i);
				FingerPrint = cf->ReadValue("oper", "fingerprint", i);
				SSLOnly = cf->ReadFlag("oper", "sslonly", i);

				if (FingerPrint.empty() && !SSLOnly)
					continue;

				if (LoginName != parameters[0])
					continue;

				if (!OneOfMatches(TheHost, TheIP, HostName.c_str()))
					continue;

				if (Password.length() && ServerInstance->PassCompare(user, Password.c_str(),parameters[1].c_str(), HashType.c_str()))
					continue;

				if (SSLOnly && !user->GetExt("ssl", dummy))
				{
					user->WriteNumeric(491, "%s :This oper login name requires an SSL connection.", user->nick.c_str());
					return 1;
				}

				/*
				 * No cert found or the fingerprint doesn't match
				 */
				if ((!cert) || (cert->GetFingerprint() != FingerPrint))
				{
					user->WriteNumeric(491, "%s :This oper login name requires a matching key fingerprint.",user->nick.c_str());
					ServerInstance->SNO->WriteToSnoMask('o',"'%s' cannot oper, does not match fingerprint", user->nick.c_str());
					ServerInstance->Logs->Log("m_ssl_oper_cert",DEFAULT,"OPER: Failed oper attempt by %s!%s@%s: credentials valid, but wrong fingerprint.", user->nick.c_str(), user->ident.c_str(), user->host.c_str());
					return 1;
				}
			}
		}

		// Let core handle it for extra stuff
		return 0;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleOperSSLCert)

