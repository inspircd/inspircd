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
	SSLCertExt(Module* parent) : ExtensionItem(EXTENSIBLE_USER, "ssl_cert", parent) {}
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

		if (!target)
		{
			user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nickname", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}
		bool operonlyfp = ServerInstance->Config->GetTag("sslinfo")->getBool("operonly");
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

class UserCertificateProviderImpl : public UserCertificateProvider
{
 public:
	CommandSSLInfo cmd;
	UserCertificateProviderImpl(Module* me) : UserCertificateProvider(me, "sslinfo"), cmd(me) {}

	ssl_cert* GetCert(User* u)
	{
		return cmd.CertExt.get(u);
	}

	std::string GetFingerprint(User* u)
	{
		ssl_cert* c = GetCert(u);
		return c ? c->GetFingerprint() : "";
	}
};

class ModuleSSLInfo : public Module
{
 public:
	UserCertificateProviderImpl prov;
	ModuleSSLInfo() : prov(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(prov);
		ServerInstance->Modules->AddService(prov.cmd);
		ServerInstance->Modules->AddService(prov.cmd.CertExt);

		Implementation eventlist[] = { I_OnWhois, I_OnPermissionCheck, I_OnSetConnectClass, I_OnUserConnect, I_OnPostConnect };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	Version GetVersion()
	{
		return Version("SSL Certificate Utilities", VF_VENDOR);
	}

	void OnWhois(User* source, User* dest)
	{
		ssl_cert* cert = prov.cmd.CertExt.get(dest);
		if (cert)
		{
			ServerInstance->SendWhoisLine(source, dest, 671, "%s %s :is using a secure connection", source->nick.c_str(), dest->nick.c_str());
			bool operonlyfp = ServerInstance->Config->GetTag("sslinfo")->getBool("operonly");
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

	void OnPermissionCheck(PermissionData& perm)
	{
		if (perm.name != "oper")
			return;
		OperPermissionData& operm = static_cast<OperPermissionData&>(perm);

		if (!operm.oper)
			return;

		ssl_cert* cert = prov.cmd.CertExt.get(perm.user);
		ConfigTag* tag = operm.oper->config_blocks[0];

		std::string fplist = tag->getString("fingerprint");
		if ((!fplist.empty() || tag->getBool("sslonly")) && !cert)
		{
			perm.reason = "SSL connection required";
			perm.result = MOD_RES_DENY;
			return;
		}

		if (!fplist.empty())
		{
			std::string myfp = cert->GetFingerprint();
			irc::spacesepstream fprints(fplist);
			std::string fingerprint;
			while (fprints.GetToken(fingerprint))
			{
				if (fingerprint == myfp)
					return;
			}
			perm.reason = "SSL fingerprint mismatch";
			perm.result = MOD_RES_DENY;
		}
	}

	void OnUserConnect(LocalUser* user)
	{
		IOHook* ioh = user->eh->GetIOHook();
		if (ioh && ioh->creator->ModuleSourceFile.find("_ssl_") != std::string::npos)
		{
			prov.cmd.CertExt.set(user, static_cast<SSLIOHook*>(ioh)->GetCertificate());
		}
	}

	void OnPostConnect(User* user)
	{
		ssl_cert *cert = prov.cmd.CertExt.get(user);
		if (!cert || cert->fingerprint.empty())
			return;
		// find an auto-oper block for this user
		for(OperIndex::iterator i = ServerInstance->Config->oper_blocks.begin(); i != ServerInstance->Config->oper_blocks.end(); i++)
		{
			OperInfo* ifo = i->second;
			std::string fp = ifo->config_blocks[0]->getString("fingerprint");
			if (fp == cert->fingerprint && ifo->config_blocks[0]->getBool("autologin"))
				user->Oper(ifo);
		}
	}

	ModResult OnSetConnectClass(LocalUser* user, ConnectClass* myclass)
	{
		ssl_cert* cert = NULL;
		IOHook* ioh = user->eh->GetIOHook();
		if (ioh && ioh->creator->ModuleSourceFile.find("_ssl_") != std::string::npos)
			cert = static_cast<SSLIOHook*>(ioh)->GetCertificate();

		bool ok = true;
		if (myclass->config->getString("requiressl") == "trusted")
		{
			ok = (cert && cert->IsCAVerified());
		}
		else if (myclass->config->getBool("requiressl"))
		{
			ok = (cert != NULL);
		}

		if (!ok)
			return MOD_RES_DENY;
		return MOD_RES_PASSTHRU;
	}

};

MODULE_INIT(ModuleSSLInfo)

