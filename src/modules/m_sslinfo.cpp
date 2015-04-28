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
#include "modules/ssl.h"

class SSLCertExt : public ExtensionItem {
 public:
	SSLCertExt(Module* parent)
		: ExtensionItem("ssl_cert", ExtensionItem::EXT_USER, parent)
	{
	}

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
			user->WriteNumeric(ERR_NOSUCHNICK, "%s :No such nickname", parameters[0].c_str());
			return CMD_FAILURE;
		}
		bool operonlyfp = ServerInstance->Config->ConfValue("sslinfo")->getBool("operonly");
		if (operonlyfp && !user->IsOper() && target != user)
		{
			user->WriteNotice("*** You cannot view SSL certificate information for other users");
			return CMD_FAILURE;
		}
		ssl_cert* cert = CertExt.get(target);
		if (!cert)
		{
			user->WriteNotice("*** No SSL certificate for this user");
		}
		else if (cert->GetError().length())
		{
			user->WriteNotice("*** No SSL certificate information for this user (" + cert->GetError() + ").");
		}
		else
		{
			user->WriteNotice("*** Distinguished Name: " + cert->GetDN());
			user->WriteNotice("*** Issuer:             " + cert->GetIssuer());
			user->WriteNotice("*** Key Fingerprint:    " + cert->GetFingerprint());
		}
		return CMD_SUCCESS;
	}
};

class UserCertificateAPIImpl : public UserCertificateAPIBase
{
	SSLCertExt& ext;

 public:
	UserCertificateAPIImpl(Module* mod, SSLCertExt& certext)
		: UserCertificateAPIBase(mod), ext(certext)
	{
	}

 	ssl_cert* GetCertificate(User* user) CXX11_OVERRIDE
 	{
 		return ext.get(user);
 	}
};

class ModuleSSLInfo : public Module, public Whois::EventListener
{
	CommandSSLInfo cmd;
	UserCertificateAPIImpl APIImpl;

 public:
	ModuleSSLInfo()
		: Whois::EventListener(this)
		, cmd(this)
		, APIImpl(this, cmd.CertExt)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("SSL Certificate Utilities", VF_VENDOR);
	}

	void OnWhois(Whois::Context& whois) CXX11_OVERRIDE
	{
		ssl_cert* cert = cmd.CertExt.get(whois.GetTarget());
		if (cert)
		{
			whois.SendLine(671, ":is using a secure connection");
			bool operonlyfp = ServerInstance->Config->ConfValue("sslinfo")->getBool("operonly");
			if ((!operonlyfp || whois.IsSelfWhois() || whois.GetSource()->IsOper()) && !cert->fingerprint.empty())
				whois.SendLine(276, ":has client certificate fingerprint %s", cert->fingerprint.c_str());
		}
	}

	ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, LocalUser *user, bool validated, const std::string &original_line) CXX11_OVERRIDE
	{
		if ((command == "OPER") && (validated))
		{
			ServerConfig::OperIndex::const_iterator i = ServerInstance->Config->oper_blocks.find(parameters[0]);
			if (i != ServerInstance->Config->oper_blocks.end())
			{
				OperInfo* ifo = i->second;
				ssl_cert* cert = cmd.CertExt.get(user);

				if (ifo->oper_block->getBool("sslonly") && !cert)
				{
					user->WriteNumeric(491, ":This oper login requires an SSL connection.");
					user->CommandFloodPenalty += 10000;
					return MOD_RES_DENY;
				}

				std::string fingerprint;
				if (ifo->oper_block->readString("fingerprint", fingerprint) && (!cert || cert->GetFingerprint() != fingerprint))
				{
					user->WriteNumeric(491, ":This oper login requires a matching SSL certificate fingerprint.");
					user->CommandFloodPenalty += 10000;
					return MOD_RES_DENY;
				}
			}
		}

		// Let core handle it for extra stuff
		return MOD_RES_PASSTHRU;
	}

	void OnUserConnect(LocalUser* user) CXX11_OVERRIDE
	{
		ssl_cert* cert = SSLClientCert::GetCertificate(&user->eh);
		if (cert)
			cmd.CertExt.set(user, cert);
	}

	void OnPostConnect(User* user) CXX11_OVERRIDE
	{
		ssl_cert *cert = cmd.CertExt.get(user);
		if (!cert || cert->fingerprint.empty())
			return;
		// find an auto-oper block for this user
		for (ServerConfig::OperIndex::const_iterator i = ServerInstance->Config->oper_blocks.begin(); i != ServerInstance->Config->oper_blocks.end(); ++i)
		{
			OperInfo* ifo = i->second;
			std::string fp = ifo->oper_block->getString("fingerprint");
			if (fp == cert->fingerprint && ifo->oper_block->getBool("autologin"))
				user->Oper(ifo);
		}
	}

	ModResult OnSetConnectClass(LocalUser* user, ConnectClass* myclass) CXX11_OVERRIDE
	{
		ssl_cert* cert = SSLClientCert::GetCertificate(&user->eh);
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
