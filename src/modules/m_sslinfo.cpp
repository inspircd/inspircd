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

/* $ModDesc: Provides SSL metadata, including /WHOIS information and /SSLINFO command */

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
					user->WriteServ("NOTICE %s :*** No SSL certificate information for this user (%s).", user->nick.c_str(), cert->GetError().c_str());
				}
				else
				{
					user->WriteServ("NOTICE %s :*** Distinguised Name: %s", user->nick.c_str(), cert->GetDN().c_str());
					user->WriteServ("NOTICE %s :*** Issuer:            %s", user->nick.c_str(), cert->GetIssuer().c_str());
					user->WriteServ("NOTICE %s :*** Key Fingerprint:   %s", user->nick.c_str(), cert->GetFingerprint().c_str());
				}
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

		Implementation eventlist[] = { I_OnSyncUserMetaData, I_OnDecodeMetaData, I_OnWhois };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}


	virtual ~ModuleSSLInfo()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}

	virtual void OnWhois(User* source, User* dest)
	{
		if(dest->GetExt("ssl"))
		{
			ServerInstance->SendWhoisLine(source, dest, 320, "%s %s :is using a secure connection", source->nick.c_str(), dest->nick.c_str());
		}
	}

	virtual void OnSyncUserMetaData(User* user, Module* proto, void* opaque, const std::string &extname, bool displayable)
	{
		// check if the linking module wants to know about OUR metadata
		if (extname == "ssl")
		{
			// check if this user has an ssl field to send
			if (!user->GetExt(extname))
				return;

			// call this function in the linking module, let it format the data how it
			// sees fit, and send it on its way. We dont need or want to know how.
			proto->ProtoSendMetaData(opaque, TYPE_USER, user, extname, displayable ? "Enabled" : "ON");
		}
		else if (extname == "ssl_cert")
		{
			ssl_cert* cert;
			if (!user->GetExt("ssl_cert", cert))
				return;

			proto->ProtoSendMetaData(opaque, TYPE_USER, user, extname, cert->GetMetaLine().c_str());
		}
	}

	virtual void OnDecodeMetaData(int target_type, void* target, const std::string &extname, const std::string &extdata)
	{
		// check if its our metadata key, and its associated with a user
		if ((target_type == TYPE_USER) && (extname == "ssl"))
		{
			User* dest = static_cast<User*>(target);
			// if they dont already have an ssl flag, accept the remote server's
			if (!dest->GetExt(extname))
			{
				dest->Extend(extname);
			}
		}
		else if ((target_type == TYPE_USER) && (extname == "ssl_cert"))
		{
			User* dest = static_cast<User*>(target);
			if (dest->GetExt(extname))
				return;

			ssl_cert* cert = new ssl_cert;
			dest->Extend(extname, cert);

			std::stringstream s(extdata);
			std::string v;
			getline(s,v,' ');

			cert->invalid = (v.find('v') != std::string::npos);
			cert->trusted = (v.find('T') != std::string::npos);
			cert->revoked = (v.find('R') != std::string::npos);
			cert->unknownsigner = (v.find('s') != std::string::npos);
			if (v.find('E') != std::string::npos)
			{
				getline(s,cert->error,'\n');
			}
			else
			{
				getline(s,cert->fingerprint,' ');
				getline(s,cert->dn,' ');
				getline(s,cert->issuer,'\n');
			}
		}
	}
};

MODULE_INIT(ModuleSSLInfo)

