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

class SSLCertExt : public ExtensionItem {
 public:
	SSLCertExt(Module* parent) : ExtensionItem("ssl_cert", parent) {}
	ssl_cert* get(const Extensible* item)
	{
		return static_cast<ssl_cert*>(get_raw(item));
	}
	void set(Extensible* item, ssl_cert* value)
	{
		ssl_cert* old = static_cast<ssl_cert*>(set_raw(item, value));
		delete old;
	}

	std::string serialize(SerializeFormat format, const Extensible* container, void* item)
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
		delete static_cast<ssl_cert*>(item);
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

		Extensible::Register(&cmd.CertExt);

		Implementation eventlist[] = { I_OnWhois, I_OnPreCommand };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	~ModuleSSLInfo()
	{
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

	ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, User *user, bool validated, const std::string &original_line)
	{
		irc::string pcmd = command.c_str();

		if ((pcmd == "OPER") && (validated))
		{
			ConfigReader cf;
			char TheHost[MAXBUF];
			char TheIP[MAXBUF];
			std::string LoginName;
			std::string Password;
			std::string OperType;
			std::string HostName;
			std::string HashType;
			std::string FingerPrint;
			bool SSLOnly;
			ssl_cert* cert = cmd.CertExt.get(user);

			snprintf(TheHost,MAXBUF,"%s@%s",user->ident.c_str(),user->host.c_str());
			snprintf(TheIP, MAXBUF,"%s@%s",user->ident.c_str(),user->GetIPString());

			for (int i = 0; i < cf.Enumerate("oper"); i++)
			{
				LoginName = cf.ReadValue("oper", "name", i);
				Password = cf.ReadValue("oper", "password", i);
				OperType = cf.ReadValue("oper", "type", i);
				HostName = cf.ReadValue("oper", "host", i);
				HashType = cf.ReadValue("oper", "hash", i);
				FingerPrint = cf.ReadValue("oper", "fingerprint", i);
				SSLOnly = cf.ReadFlag("oper", "sslonly", i);

				if (FingerPrint.empty() && !SSLOnly)
					continue;

				if (LoginName != parameters[0])
					continue;

				if (!OneOfMatches(TheHost, TheIP, HostName.c_str()))
					continue;

				if (Password.length() && ServerInstance->PassCompare(user, Password.c_str(),parameters[1].c_str(), HashType.c_str()))
					continue;

				if (SSLOnly && !cert)
				{
					user->WriteNumeric(491, "%s :This oper login name requires an SSL connection.", user->nick.c_str());
					return MOD_RES_DENY;
				}

				/*
				 * No cert found or the fingerprint doesn't match
				 */
				if ((!cert) || (cert->GetFingerprint() != FingerPrint))
				{
					user->WriteNumeric(491, "%s :This oper login name requires a matching key fingerprint.",user->nick.c_str());
					ServerInstance->SNO->WriteToSnoMask('o',"'%s' cannot oper, does not match fingerprint", user->nick.c_str());
					ServerInstance->Logs->Log("m_ssl_oper_cert",DEFAULT,"OPER: Failed oper attempt by %s!%s@%s: credentials valid, but wrong fingerprint.", user->nick.c_str(), user->ident.c_str(), user->host.c_str());
					return MOD_RES_DENY;
				}
			}
		}

		// Let core handle it for extra stuff
		return MOD_RES_PASSTHRU;
	}

	const char* OnRequest(Request* request)
	{
		if (strcmp("GET_CERT", request->GetId()) == 0)
		{
			BufferedSocketCertificateRequest* req = static_cast<BufferedSocketCertificateRequest*>(request);
			req->cert = cmd.CertExt.get(req->item);
		}
		else if (strcmp("SET_CERT", request->GetId()) == 0)
		{
			BufferedSocketFingerprintSubmission* req = static_cast<BufferedSocketFingerprintSubmission*>(request);
			cmd.CertExt.set(req->item, req->cert);
		}
		return NULL;
	}
};

MODULE_INIT(ModuleSSLInfo)

