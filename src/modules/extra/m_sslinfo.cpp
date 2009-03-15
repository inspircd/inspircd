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

#include "inspircd.h"
#include "transport.h"

/* $ModDesc: Provides /sslinfo command used to test who a mask matches */
/* $ModDep: transport.h */

/** Handle /SSLINFO
 */
class CommandSSLInfo : public Command
{
 public:
	CommandSSLInfo (InspIRCd* Instance) : Command(Instance,"SSLINFO", 0, 1)
	{
		this->source = "m_sslinfo.so";
		this->syntax = "<nick>";
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		User* target = ServerInstance->FindNick(parameters[0]);
		ssl_cert* cert;

		if (target)
		{
			if (target->GetExt("ssl_cert", cert))
			{
				if (cert->GetError().length())
				{
					user->WriteServ("NOTICE %s :*** Error:             %s", user->nick.c_str(), cert->GetError().c_str());
				}
				user->WriteServ("NOTICE %s :*** Distinguised Name: %s", user->nick.c_str(), cert->GetDN().c_str());
				user->WriteServ("NOTICE %s :*** Issuer:            %s", user->nick.c_str(), cert->GetIssuer().c_str());
				user->WriteServ("NOTICE %s :*** Key Fingerprint:   %s", user->nick.c_str(), cert->GetFingerprint().c_str());
				return CMD_LOCALONLY;
			}
			else
			{
				user->WriteServ("NOTICE %s :*** No SSL certificate information for this user.", user->nick.c_str());
				return CMD_FAILURE;
			}
		}
		else
			user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nickname", user->nick.c_str(), parameters[0].c_str());

		return CMD_FAILURE;
	}
};

class ModuleSSLInfo : public Module
{
	CommandSSLInfo* newcommand;
 public:
	ModuleSSLInfo(InspIRCd* Me)
		: Module(Me)
	{

		newcommand = new CommandSSLInfo(ServerInstance);
		ServerInstance->AddCommand(newcommand);

	}


	virtual ~ModuleSSLInfo()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleSSLInfo)

