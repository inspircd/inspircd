/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Molly Miller
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2017-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2010 Adam <Adam@anope.org>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007, 2009 Craig Edwards <brain@inspircd.org>
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
#include "modules/who.h"

class SSLCertExt : public ExtensionItem
{
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
		free(container, unset_raw(container));
	}

	std::string ToNetwork(const Extensible* container, void* item) const CXX11_OVERRIDE
	{
		return static_cast<ssl_cert*>(item)->GetMetaLine();
	}

	void FromNetwork(Extensible* container, const std::string& value) CXX11_OVERRIDE
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

class UserCertificateAPIImpl : public UserCertificateAPIBase
{
 public:
	LocalIntExt nosslext;
	SSLCertExt sslext;

	UserCertificateAPIImpl(Module* mod)
		: UserCertificateAPIBase(mod)
		, nosslext("no_ssl_cert", ExtensionItem::EXT_USER, mod)
		, sslext(mod)
	{
	}

	ssl_cert* GetCertificate(User* user) CXX11_OVERRIDE
	{
		ssl_cert* cert = sslext.get(user);
		if (cert)
			return cert;

		LocalUser* luser = IS_LOCAL(user);
		if (!luser || nosslext.get(luser))
			return NULL;

		cert = SSLClientCert::GetCertificate(&luser->eh);
		if (!cert)
			return NULL;

		SetCertificate(user, cert);
		return cert;
	}

	void SetCertificate(User* user, ssl_cert* cert) CXX11_OVERRIDE
	{
		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Setting TLS (SSL) client certificate for %s: %s",
			user->GetFullHost().c_str(), cert->GetMetaLine().c_str());
		sslext.set(user, cert);
	}
};

class CommandSSLInfo : public SplitCommand
{
 private:
	ChanModeReference sslonlymode;

	void HandleUserInternal(LocalUser* source, User* target, bool verbose)
	{
		ssl_cert* cert = sslapi.GetCertificate(target);
		if (!cert)
		{
			source->WriteNotice(InspIRCd::Format("*** %s is not connected using TLS (SSL).", target->nick.c_str()));
		}
		else if (cert->GetError().length())
		{
			source->WriteNotice(InspIRCd::Format("*** %s is connected using TLS (SSL) but has not specified a valid client certificate (%s).",
				target->nick.c_str(), cert->GetError().c_str()));
		}
		else if (!verbose)
		{
			source->WriteNotice(InspIRCd::Format("*** %s is connected using TLS (SSL) with a valid client certificate (%s).",
				target->nick.c_str(), cert->GetFingerprint().c_str()));
		}
		else
		{
			source->WriteNotice("*** Distinguished Name: " + cert->GetDN());
			source->WriteNotice("*** Issuer:             " + cert->GetIssuer());
			source->WriteNotice("*** Key Fingerprint:    " + cert->GetFingerprint());
		}
	}

	CmdResult HandleUser(LocalUser* source, const std::string& nick)
	{
		User* target = ServerInstance->FindNickOnly(nick);
		if (!target || target->registered != REG_ALL)
		{
			source->WriteNumeric(Numerics::NoSuchNick(nick));
			return CMD_FAILURE;
		}

		if (operonlyfp && !source->IsOper() && source != target)
		{
			source->WriteNumeric(ERR_NOPRIVILEGES, "You must be a server operator to view TLS (SSL) client certificate information for other users.");
			return CMD_FAILURE;
		}

		HandleUserInternal(source, target, true);
		return CMD_SUCCESS;
	}

	CmdResult HandleChannel(LocalUser* source, const std::string& channel)
	{
		Channel* chan = ServerInstance->FindChan(channel);
		if (!chan)
		{
			source->WriteNumeric(Numerics::NoSuchChannel(channel));
			return CMD_FAILURE;
		}

		if (operonlyfp && !source->IsOper())
		{
			source->WriteNumeric(ERR_NOPRIVILEGES, "You must be a server operator to view TLS (SSL) client certificate information for channels.");
			return CMD_FAILURE;
		}

		if (!source->IsOper() && chan->GetPrefixValue(source) < OP_VALUE)
		{
			source->WriteNumeric(Numerics::ChannelPrivilegesNeeded(chan, OP_VALUE, "view TLS (SSL) client certificate information"));
			return CMD_FAILURE;
		}

		if (sslonlymode)
		{
			source->WriteNotice(InspIRCd::Format("*** %s %s have channel mode +%c (%s) set.",
				chan->name.c_str(), chan->IsModeSet(sslonlymode) ? "does" : "does not",
				sslonlymode->GetModeChar(), sslonlymode->name.c_str()));
		}

		const Channel::MemberMap& userlist = chan->GetUsers();
		for (Channel::MemberMap::const_iterator i = userlist.begin(); i != userlist.end(); ++i)
		{
			if (!i->first->server->IsULine())
				HandleUserInternal(source, i->first, false);
		}

		return CMD_SUCCESS;
	}

 public:
	UserCertificateAPIImpl sslapi;
	bool operonlyfp;

	CommandSSLInfo(Module* Creator)
		: SplitCommand(Creator, "SSLINFO", 1)
		, sslonlymode(Creator, "sslonly")
		, sslapi(Creator)
	{
		allow_empty_last_param = false;
		syntax = "<channel|nick>";
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) CXX11_OVERRIDE
	{
		if (ServerInstance->IsChannel(parameters[0]))
			return HandleChannel(user, parameters[0]);
		else
			return HandleUser(user, parameters[0]);
	}
};

class ModuleSSLInfo
	: public Module
	, public WebIRC::EventListener
	, public Whois::EventListener
	, public Who::EventListener
{
 private:
	CommandSSLInfo cmd;
	std::string hash;
	unsigned long warnexpiring;

	bool MatchFP(ssl_cert* const cert, const std::string& fp) const
	{
		return irc::spacesepstream(fp).Contains(cert->GetFingerprint());
	}

 public:
	ModuleSSLInfo()
		: WebIRC::EventListener(this)
		, Whois::EventListener(this)
		, Who::EventListener(this)
		, cmd(this)
	{
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("sslinfo");
		cmd.operonlyfp = tag->getBool("operonly");
		hash = tag->getString("hash");
		warnexpiring = tag->getDuration("warnexpiring", 0, 0, 60*60*24*365);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Adds user facing TLS (SSL) information, various TLS (SSL) configuration options, and the /SSLINFO command to look up TLS (SSL) certificate information for other users.", VF_VENDOR);
	}

	void OnWhois(Whois::Context& whois) CXX11_OVERRIDE
	{
		ssl_cert* cert = cmd.sslapi.GetCertificate(whois.GetTarget());
		if (cert)
		{
			whois.SendLine(RPL_WHOISSECURE, "is using a secure connection");
			if ((!cmd.operonlyfp || whois.IsSelfWhois() || whois.GetSource()->IsOper()) && !cert->fingerprint.empty())
				whois.SendLine(RPL_WHOISCERTFP, InspIRCd::Format("has TLS (SSL) client certificate fingerprint %s", cert->fingerprint.c_str()));
		}
	}

	ModResult OnWhoLine(const Who::Request& request, LocalUser* source, User* user, Membership* memb, Numeric::Numeric& numeric) CXX11_OVERRIDE
	{
		size_t flag_index;
		if (!request.GetFieldIndex('f', flag_index))
			return MOD_RES_PASSTHRU;

		ssl_cert* cert = cmd.sslapi.GetCertificate(user);
		if (cert)
			numeric.GetParams()[flag_index].push_back('s');

		return MOD_RES_PASSTHRU;
	}

	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) CXX11_OVERRIDE
	{
		if ((command == "OPER") && (validated))
		{
			ServerConfig::OperIndex::const_iterator i = ServerInstance->Config->oper_blocks.find(parameters[0]);
			if (i != ServerInstance->Config->oper_blocks.end())
			{
				OperInfo* ifo = i->second;
				ssl_cert* cert = cmd.sslapi.GetCertificate(user);

				if (ifo->oper_block->getBool("sslonly") && !cert)
				{
					user->WriteNumeric(ERR_NOOPERHOST, "Invalid oper credentials");
					user->CommandFloodPenalty += 10000;
					ServerInstance->SNO->WriteGlobalSno('o', "WARNING! Failed oper attempt by %s using login '%s': a secure connection is required.", user->GetFullRealHost().c_str(), parameters[0].c_str());
					return MOD_RES_DENY;
				}

				std::string fingerprint;
				if (ifo->oper_block->readString("fingerprint", fingerprint) && (!cert || !MatchFP(cert, fingerprint)))
				{
					user->WriteNumeric(ERR_NOOPERHOST, "Invalid oper credentials");
					user->CommandFloodPenalty += 10000;
					ServerInstance->SNO->WriteGlobalSno('o', "WARNING! Failed oper attempt by %s using login '%s': their TLS (SSL) client certificate fingerprint does not match.", user->GetFullRealHost().c_str(), parameters[0].c_str());
					return MOD_RES_DENY;
				}
			}
		}

		// Let core handle it for extra stuff
		return MOD_RES_PASSTHRU;
	}

	void OnPostConnect(User* user) CXX11_OVERRIDE
	{
		LocalUser* const localuser = IS_LOCAL(user);
		if (!localuser)
			return;

		const SSLIOHook* const ssliohook = SSLIOHook::IsSSL(&localuser->eh);
		if (!ssliohook || cmd.sslapi.nosslext.get(localuser))
			return;

		ssl_cert* const cert = ssliohook->GetCertificate();

		std::string text = "*** You are connected to ";
		if (!ssliohook->GetServerName(text))
			text.append(ServerInstance->Config->GetServerName());
		text.append(" using TLS (SSL) cipher '");
		ssliohook->GetCiphersuite(text);
		text.push_back('\'');
		if (cert && !cert->GetFingerprint().empty())
			text.append(" and your TLS (SSL) client certificate fingerprint is ").append(cert->GetFingerprint());
		user->WriteNotice(text);

		if (!cert)
			return;

		// Find an auto-oper block for this user
		for (ServerConfig::OperIndex::const_iterator i = ServerInstance->Config->oper_blocks.begin(); i != ServerInstance->Config->oper_blocks.end(); ++i)
		{
			OperInfo* ifo = i->second;
			std::string fp = ifo->oper_block->getString("fingerprint");
			if (!MatchFP(cert, fp))
				continue;

			bool do_login = false;
			const std::string autologin = ifo->oper_block->getString("autologin");
			if (stdalgo::string::equalsci(autologin, "if-host-match"))
			{
				const std::string& userHost = localuser->MakeHost();
				const std::string& userIP = localuser->MakeHostIP();
				do_login = InspIRCd::MatchMask(ifo->oper_block->getString("host"), userHost, userIP);
			}
			else if (ifo->oper_block->getBool("autologin"))
			{
				do_login = true;
			}

			if (do_login)
				user->Oper(ifo);
		}

		if (!warnexpiring || !cert->GetExpirationTime())
			return;

		if (ServerInstance->Time() > cert->GetExpirationTime())
		{
			user->WriteNotice("*** Your TLS (SSL) client certificate has expired.");
		}
		else if (static_cast<time_t>(ServerInstance->Time() + warnexpiring) > cert->GetExpirationTime())
		{
			const std::string duration = InspIRCd::DurationString(cert->GetExpirationTime() - ServerInstance->Time());
			user->WriteNotice("*** Your TLS (SSL) client certificate expires in " + duration + ".");
		}
	}

	ModResult OnSetConnectClass(LocalUser* user, ConnectClass* myclass) CXX11_OVERRIDE
	{
		ssl_cert* cert = cmd.sslapi.GetCertificate(user);
		const char* error = NULL;
		const std::string requiressl = myclass->config->getString("requiressl");
		if (stdalgo::string::equalsci(requiressl, "trusted"))
		{
			if (!cert || !cert->IsCAVerified())
				error = "a trusted TLS (SSL) client certificate";
		}
		else if (myclass->config->getBool("requiressl"))
		{
			if (!cert)
				error = "a TLS (SSL) connection";
		}

		if (error)
		{
			ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG, "The %s connect class is not suitable as it requires %s",
				myclass->GetName().c_str(), error);
			return MOD_RES_DENY;
		}

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
		if (!cmd.sslapi.GetCertificate(user))
			return;

		WebIRC::FlagMap::const_iterator iter = flags->find("secure");
		if (iter == flags->end())
		{
			// If this is not set then the connection between the client and
			// the gateway is not secure.
			cmd.sslapi.nosslext.set(user, 1);
			cmd.sslapi.sslext.unset(user);
			return;
		}

		// Create a fake ssl_cert for the user.
		ssl_cert* cert = new ssl_cert;
		if (!hash.empty())
		{
			iter = flags->find("certfp-" + hash);
			if (iter != flags->end() && !iter->second.empty())
			{
				// If the gateway specifies this flag we put all trust onto them
				// for having validated the client certificate. This is probably
				// ill-advised but there's not much else we can do.
				cert->fingerprint = iter->second;
				cert->dn = "(unknown)";
				cert->invalid = false;
				cert->issuer = "(unknown)";
				cert->trusted = true;
				cert->unknownsigner = false;
			}
		}

		if (cert->fingerprint.empty())
		{
			cert->error = "WebIRC gateway did not send a client fingerprint";
			cert->revoked = true;
		}

		cmd.sslapi.SetCertificate(user, cert);
	}
};

MODULE_INIT(ModuleSSLInfo)
