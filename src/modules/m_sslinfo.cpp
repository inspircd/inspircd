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
#include "modules/webirc.h"
#include "modules/whois.h"

enum
{
	// From oftc-hybrid.
	RPL_WHOISCERTFP = 276,

	// From UnrealIRCd.
	RPL_WHOISSECURE = 671
};

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

	void unset(Extensible* container)
	{
		void* old = unset_raw(container);
		delete static_cast<std::string*>(old);
	}

	std::string serialize(SerializeFormat format, const Extensible* container, void* item) const CXX11_OVERRIDE
	{
		return static_cast<ssl_cert*>(item)->GetMetaLine();
	}

	void unserialize(SerializeFormat format, Extensible* container, const std::string& value) CXX11_OVERRIDE
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

	void free(Extensible* container, void* item) CXX11_OVERRIDE
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

	CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		User* target = ServerInstance->FindNickOnly(parameters[0]);

		if ((!target) || (target->registered != REG_ALL))
		{
			user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
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

	void SetCertificate(User* user, ssl_cert* cert) CXX11_OVERRIDE
	{
		ext.set(user, cert);
	}
};

class ModuleSSLInfo
	: public Module
	, public WebIRC::EventListener
	, public Whois::EventListener
{
	CommandSSLInfo cmd;
	UserCertificateAPIImpl APIImpl;

 public:
	ModuleSSLInfo()
		: WebIRC::EventListener(this)
		, Whois::EventListener(this)
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
			whois.SendLine(RPL_WHOISSECURE, "is using a secure connection");
			bool operonlyfp = ServerInstance->Config->ConfValue("sslinfo")->getBool("operonly");
			if ((!operonlyfp || whois.IsSelfWhois() || whois.GetSource()->IsOper()) && !cert->fingerprint.empty())
				whois.SendLine(RPL_WHOISCERTFP, InspIRCd::Format("has client certificate fingerprint %s", cert->fingerprint.c_str()));
		}
	}

	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) CXX11_OVERRIDE
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
					user->WriteNumeric(ERR_NOOPERHOST, "This oper login requires an SSL connection.");
					user->CommandFloodPenalty += 10000;
					return MOD_RES_DENY;
				}

				std::string fingerprint;
				if (ifo->oper_block->readString("fingerprint", fingerprint) && (!cert || cert->GetFingerprint() != fingerprint))
				{
					user->WriteNumeric(ERR_NOOPERHOST, "This oper login requires a matching SSL certificate fingerprint.");
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
		LocalUser* const localuser = IS_LOCAL(user);
		if (!localuser)
			return;

		const SSLIOHook* const ssliohook = SSLIOHook::IsSSL(&localuser->eh);
		if (!ssliohook)
			return;

		ssl_cert* const cert = ssliohook->GetCertificate();

		{
			std::string text = "*** You are connected to ";
			if (!ssliohook->GetServerName(text))
				text.append(ServerInstance->Config->ServerName);
			text.append(" using SSL cipher '");
			ssliohook->GetCiphersuite(text);
			text.push_back('\'');
			if ((cert) && (!cert->GetFingerprint().empty()))
				text.append(" and your SSL certificate fingerprint is ").append(cert->GetFingerprint());
			user->WriteNotice(text);
		}

		if (!cert)
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
			ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG, "Class requires a trusted SSL cert. Client %s one.", (ok ? "has" : "does not have"));
		}
		else if (myclass->config->getBool("requiressl"))
		{
			ok = (cert != NULL);
			ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG, "Class requires any SSL cert. Client %s one.", (ok ? "has" : "does not have"));
		}

		if (!ok)
			return MOD_RES_DENY;
		return MOD_RES_PASSTHRU;
	}

	void OnWebIRCAuth(LocalUser* user, const WebIRC::FlagMap* flags) CXX11_OVERRIDE
	{
		// We are only interested in connection flags. If none have been
		// given then we have nothing to do.
		if (!flags)
			return;

		// We only care about the tls connection flag if the connection
		// between the gateway and the server is secure.
		if (!cmd.CertExt.get(user))
			return;

		WebIRC::FlagMap::const_iterator iter = flags->find("secure");
		if (iter == flags->end())
		{
			// If this is not set then the connection between the client and
			// the gateway is not secure.
			cmd.CertExt.unset(user);
			return;
		}

		// Create a fake ssl_cert for the user.
		ssl_cert* cert = new ssl_cert;
		cert->error = "WebIRC users can not specify valid certs yet";
		cert->invalid = true;
		cert->revoked = true;
		cert->trusted = false;
		cert->unknownsigner = true;
		cmd.CertExt.set(user, cert);
	}
};

MODULE_INIT(ModuleSSLInfo)
