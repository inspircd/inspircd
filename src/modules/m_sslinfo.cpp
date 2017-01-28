/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
		User* target = ServerInstance->FindNickOnly(parameters[0]);

		if ((!target) || (target->registered != REG_ALL))
		{
			user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nickname", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}
		bool operonlyfp = ServerInstance->Config->ConfValue("sslinfo")->getBool("operonly");
		if (operonlyfp && !IS_OPER(user) && target != user)
		{
			user->WriteServ("NOTICE %s :*** You cannot view SSL certificate information for other users", user->nick.c_str());
			return CMD_FAILURE;
		}
		ssl_cert* cert = CertExt.get(target);
		if (!cert)
		{
			user->WriteServ("NOTICE %s :*** No SSL certificate for this user", user->nick.c_str());
		}
		else if (cert->GetError().length())
		{
			user->WriteServ("NOTICE %s :*** No SSL certificate information for this user (%s).", user->nick.c_str(), cert->GetError().c_str());
		}
		else
		{
			user->WriteServ("NOTICE %s :*** Distinguished Name: %s", user->nick.c_str(), cert->GetDN().c_str());
			user->WriteServ("NOTICE %s :*** Issuer:             %s", user->nick.c_str(), cert->GetIssuer().c_str());
			user->WriteServ("NOTICE %s :*** Key Fingerprint:    %s", user->nick.c_str(), cert->GetFingerprint().c_str());
		}
		return CMD_SUCCESS;
	}
};

class ModuleSSLInfo : public Module
{
	CommandSSLInfo cmd;

 public:
	ModuleSSLInfo() : cmd(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);

		ServerInstance->Modules->AddService(cmd.CertExt);

		Implementation eventlist[] = { I_OnWhois, I_OnPreCommand, I_OnSetConnectClass, I_OnUserConnect, I_OnPostConnect };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	Version GetVersion()
	{
		return Version("SSL Certificate Utilities", VF_VENDOR);
	}

	void OnWhois(User* source, User* dest)
	{
		ssl_cert* cert = cmd.CertExt.get(dest);
		if (cert)
		{
			ServerInstance->SendWhoisLine(source, dest, 671, "%s %s :is using a secure connection", source->nick.c_str(), dest->nick.c_str());
			bool operonlyfp = ServerInstance->Config->ConfValue("sslinfo")->getBool("operonly");
			if ((!operonlyfp || source == dest || IS_OPER(source)) && !cert->fingerprint.empty())
				ServerInstance->SendWhoisLine(source, dest, 276, "%s %s :has client certificate fingerprint %s",
					source->nick.c_str(), dest->nick.c_str(), cert->fingerprint.c_str());
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
		if ((command == "OPER") && (validated))
		{
			OperIndex::iterator i = ServerInstance->Config->oper_blocks.find(parameters[0]);
			if (i != ServerInstance->Config->oper_blocks.end())
			{
				OperInfo* ifo = i->second;
				if (!ifo->oper_block)
					return MOD_RES_PASSTHRU;

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

	void OnUserConnect(LocalUser* user)
	{
		SocketCertificateRequest req(&user->eh, this);
		if (!req.cert)
			return;
		cmd.CertExt.set(user, req.cert);
	}

	void OnPostConnect(User* user)
	{
		ssl_cert *cert = cmd.CertExt.get(user);
		if (!cert || cert->fingerprint.empty())
			return;
		// find an auto-oper block for this user
		for(OperIndex::iterator i = ServerInstance->Config->oper_blocks.begin(); i != ServerInstance->Config->oper_blocks.end(); i++)
		{
			OperInfo* ifo = i->second;
			if (!ifo->oper_block)
				continue;

			std::string fp = ifo->oper_block->getString("fingerprint");
			if (fp == cert->fingerprint && ifo->oper_block->getBool("autologin"))
				user->Oper(ifo);
		}
	}

	ModResult OnSetConnectClass(LocalUser* user, ConnectClass* myclass)
	{
		SocketCertificateRequest req(&user->eh, this);
		bool ok = true;
		if (myclass->config->getString("requiressl") == "trusted")
		{
			ok = (req.cert && req.cert->IsCAVerified());
			if (!ok) {
				ServerInstance->Logs->Log("m_sslinfo", DEFAULT,
					"Invalid client certificate from '%s' port '%d': '%s'",
					user->GetIPString(), user->GetServerPort(),
					req.cert ? req.cert->GetError().c_str() : "No SSL in use");
			} else {
				ServerInstance->Logs->Log("m_sslinfo", DEFAULT,
					"Accepted client certificate from '%s' port '%d' with DN '%s', Issuer '%s', and FP '%s'",
					user->GetIPString(), user->GetServerPort(),
					req.cert->GetDN().c_str(),
					req.cert->GetIssuer().c_str(),
					req.cert->GetFingerprint().c_str());
			}
		}
		else if (myclass->config->getBool("requiressl"))
		{
			ok = (req.cert != NULL);
		}

		if (!ok)
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
	}
};

MODULE_INIT(ModuleSSLInfo)

