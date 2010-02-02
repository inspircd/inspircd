/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "ssl.h"

/* $ModDesc: Provides SSL metadata, including /WHOIS information and /SSLINFO command */

class SSLCertExt : public ExtensionItem {
 public:
	SSLCertExt(Module* parent) : ExtensionItem("ssl_cert", parent) {}
	ssl_cert* get(const Extensible* item) const
	{
		return static_cast<ssl_cert*>(get_raw(item));
	}
	void set(Extensible* item, ssl_cert* value)
	{
		value->refcount_inc();
		ssl_cert* old = static_cast<ssl_cert*>(set_raw(item, value));
		if (old && old->refcount_dec())
			delete old;
	}

	std::string serialize(SerializeFormat format, const Extensible* container, void* item) const
	{
		return static_cast<ssl_cert*>(item)->GetMetaLine();
	}

	void unserialize(SerializeFormat format, Extensible* container, const std::string& value)
	{
		ssl_cert* cert = new ssl_cert;
		set(container, cert);

		std::stringstream s(value);
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

	void free(void* item)
	{
		ssl_cert* old = static_cast<ssl_cert*>(item);
		if (old && old->refcount_dec())
			delete old;
	}
};

/** Handle /SSLINFO
 */
class CommandSSLInfo : public Command
{
 public:
	SSLCertExt CertExt;

	CommandSSLInfo(Module* Creator) : Command(Creator, "SSLINFO", 1), CertExt(Creator)
	{
		this->syntax = "<nick>";
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		User* target = ServerInstance->FindNick(parameters[0]);

		if (target)
		{
			ssl_cert* cert = CertExt.get(target);
			if (cert)
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
				return CMD_SUCCESS;
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
	CommandSSLInfo cmd;

 public:
	ModuleSSLInfo()
		: cmd(this)
	{
		ServerInstance->AddCommand(&cmd);

		ServerInstance->Extensions.Register(&cmd.CertExt);

		Implementation eventlist[] = { I_OnWhois, I_OnPreCommand, I_OnSetConnectClass };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	Version GetVersion()
	{
		return Version("SSL Certificate Utilities", VF_VENDOR);
	}

	void OnWhois(User* source, User* dest)
	{
		if (cmd.CertExt.get(dest))
		{
			ServerInstance->SendWhoisLine(source, dest, 320, "%s %s :is using a secure connection", source->nick.c_str(), dest->nick.c_str());
		}
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

	ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, LocalUser *user, bool validated, const std::string &original_line)
	{
		irc::string pcmd = command.c_str();

		if ((pcmd == "OPER") && (validated))
		{
			OperIndex::iterator i = ServerInstance->Config->oper_blocks.find(parameters[0]);
			if (i != ServerInstance->Config->oper_blocks.end())
			{
				OperInfo* ifo = i->second;
				ssl_cert* cert = cmd.CertExt.get(user);

				if (ifo->oper_block->getBool("sslonly") && !cert)
				{
					user->WriteNumeric(491, "%s :This oper login requires an SSL connection.", user->nick.c_str());
					user->CommandFloodPenalty += 10000;
					return MOD_RES_DENY;
				}

				std::string fingerprint;
				if (ifo->oper_block->readString("fingerprint", fingerprint) && (!cert || cert->GetFingerprint() != fingerprint))
				{
					user->WriteNumeric(491, "%s :This oper login requires a matching SSL fingerprint.",user->nick.c_str());
					user->CommandFloodPenalty += 10000;
					return MOD_RES_DENY;
				}
			}
		}

		// Let core handle it for extra stuff
		return MOD_RES_PASSTHRU;
	}

	ModResult OnSetConnectClass(LocalUser* user, ConnectClass* myclass)
	{
		if (myclass->config->getBool("requiressl") && !cmd.CertExt.get(user))
			return MOD_RES_DENY;
		return MOD_RES_PASSTHRU;
	}

	void OnRequest(Request& request)
	{
		if (strcmp("GET_USER_CERT", request.id) == 0)
		{
			UserCertificateRequest& req = static_cast<UserCertificateRequest&>(request);
			req.cert = cmd.CertExt.get(req.user);
		}
		else if (strcmp("SET_CERT", request.id) == 0)
		{
			SSLCertSubmission& req = static_cast<SSLCertSubmission&>(request);
			cmd.CertExt.set(req.item, req.cert);
		}
	}
};

MODULE_INIT(ModuleSSLInfo)

