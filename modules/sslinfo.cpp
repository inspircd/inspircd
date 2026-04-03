/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2017-2025 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013, 2015-2016 Attila Molnar <attilamolnar@hush.com>
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


#include <sstream>

#include "inspircd.h"
#include "extension.h"
#include "modules/stats.h"
#include "modules/tls.h"
#include "modules/webirc.h"
#include "modules/who.h"
#include "modules/whois.h"
#include "numerichelper.h"
#include "stringutils.h"
#include "timeutils.h"

struct RemoteCertificate final
	: public TLS::Certificate
{
	RemoteCertificate(const Percent::QueryData& data)
	{
		auto getstr = [&data](const auto& key) {
			auto it = data.find(key);
			return it == data.end() ? "" : it->second;
		};

		StringSplitter ss(getstr("fingerprints"));
		for (std::string fingerprint; ss.GetToken(fingerprint); )
			this->fingerprints.push_back(fingerprint);

		this->dn = getstr("dn");
		this->error = getstr("error");
		this->issuer_dn = getstr("issuer-dn");

		this->activation = ConvToNum<time_t>(getstr("activation"));
		this->expiration = ConvToNum<time_t>(getstr("expiration"));

		this->known_signer = !!ConvToNum<uintmax_t>(getstr("known-signer"));
		this->revoked = !!ConvToNum<uintmax_t>(getstr("revoked"));
		this->trusted = !!ConvToNum<uintmax_t>(getstr("trusted"));
		this->valid = !!ConvToNum<uintmax_t>(getstr("valid"));
	}

	RemoteCertificate(const std::string& data)
	{
		// Compatibility for the old ssl_cert metadata.
		std::stringstream stream(data);

		std::string flags;
		std::getline(stream, flags, ' ');
		this->known_signer = (flags.find('S') != std::string::npos);
		this->revoked = (flags.find('R') != std::string::npos);
		this->trusted = (flags.find('T') != std::string::npos);
		this->valid = (flags.find('V') != std::string::npos);

		if (flags.find('E') != std::string::npos)
		{
			std::getline(stream, this->error, '\n');
			return;
		}

		std::string fingerprint;
		std::getline(stream, fingerprint, ' ');
		StringSplitter ss(fingerprint, ',');
		while (ss.GetToken(fingerprint))
			this->fingerprints.push_back(fingerprint);

		std::getline(stream, this->dn, ' ');
		std::getline(stream, this->issuer_dn, '\n');
	}
};

class TLSCertificateExt final
	: public SimpleExtItem<TLS::Certificate>
{
public:
	TLSCertificateExt(const WeakModulePtr& mod)
		: SimpleExtItem<TLS::Certificate>(mod, "tls-cert", ExtensionType::USER, true)
	{
	}

	void OnSync(const Extensible* container, const ExtensionPtr& item, Server* server) override
	{
		// Compatibility for the old ssl_cert metadata.
		const auto& cert = std::static_pointer_cast<TLS::Certificate>(item);

		std::stringstream value;
		value << 'c'
			<< (cert->HasError() ? 'E' : 'e')
			<< (cert->IsTrusted() ? 'T' : 't')
			<< (cert->IsKnownSigner() ? 'S' : 's')
			<< (cert->IsRevoked() ? 'R' : 'r')
			<< (cert->IsValid() ? 'V' : 'v')
			<< ' ';

		if (cert->HasError())
			value << cert->GetError();
		else
		{
			value << insp::join(cert->GetFingerprints(), ',') << ' '
				<< cert->GetDN() << ' '
				<< cert->GetIssuerDN();
		}

		if (server)
			server->SendMetadata(container, "ssl_cert", value.str());
		else
			ServerInstance->PI->SendMetadata(container, "ssl_cert", value.str());
	}

	void FromInternal(Extensible* container, const std::string& value) noexcept override
	{
		if (container->extype != this->extype)
			return;

		const auto data = Percent::DecodeQuery(value);
		Set(container, std::make_shared<RemoteCertificate>(data), false);
	}

	std::string ToInternal(const Extensible* container, const ExtensionPtr& item) const noexcept override
	{
		const auto& cert = std::static_pointer_cast<TLS::Certificate>(item);
		return Percent::EncodeQuery({
			{ "fingerprints", insp::join(cert->GetFingerprints())  },

			{ "dn",           cert->GetDN()                        },
			{ "error",        cert->GetError()                     },
			{ "issuer-dn",    cert->GetIssuerDN()                  },

			{ "activation",   ConvToStr(cert->GetActivationTime()) },
			{ "expiration",   ConvToStr(cert->GetExpirationTime()) },

			{ "known-signer", ConvToStr(cert->IsKnownSigner())     },
			{ "revoked",      ConvToStr(cert->IsRevoked())         },
			{ "trusted",      ConvToStr(cert->IsTrusted())         },
			{ "valid",        ConvToStr(cert->IsValid())           },
		});
	}
};

class TLSAPIImpl final
	: public TLS::APIBase
{
public:
	BoolExtItem notlsext;
	TLSCertificateExt tlsext;
	bool localsecure;

	TLSAPIImpl(const WeakModulePtr& mod)
		: TLS::APIBase(mod)
		, notlsext(mod, "no-tls-cert", ExtensionType::USER)
		, tlsext(mod)
	{
	}

	TLS::Certificate* GetCertificate(User* user) override
	{
		auto* cert = tlsext.Get(user);
		if (cert)
			return cert;

		auto* luser = user->AsLocal();
		if (!luser || notlsext.Get(luser))
			return nullptr;

		auto* hook = TLS::GetHook(luser->io->GetSocket());
		if (!hook)
			return nullptr;

		auto newcert = hook->GetCertificate();
		if (!newcert)
			return nullptr;

		SetCertificate(user, newcert);
		return newcert.get();
	}

	bool IsSecure(User* user) override
	{
		const auto* cert = GetCertificate(user);
		if (cert)
			return !!cert;

		if (localsecure)
			return user->client_sa.is_local();

		return false;
	}

	void SetCertificate(User* user, const TLS::CertificatePtr& cert) override
	{
		ServerInstance->Logs.Debug(MODNAME, "Setting TLS client certificate for {}: {}",
			user->GetMask(), tlsext.ToNetwork(user, cert));
		tlsext.Set(user, cert);
	}
};

class CommandSSLInfo final
	: public SplitCommand
{
private:
	ChanModeReference sslonlymode;

	void HandleUserInternal(LocalUser* source, User* target, bool verbose)
	{
		const auto* cert = tlsapi.GetCertificate(target);
		if (!cert)
		{
			source->WriteNotice("*** {} is not connected using TLS.", target->nick);
		}
		else if (cert->HasError())
		{
			source->WriteNotice("*** {} is connected using TLS but has not specified a valid client certificate ({}).",
				target->nick, cert->GetError());
		}
		else if (!verbose)
		{
			source->WriteNotice("*** {} is connected using TLS with a valid client certificate ({}).",
				target->nick, cert->GetFingerprint());
		}
		else
		{
			source->WriteNotice("*** {} is connected using TLS.", target->nick);

			const auto can_see_full = source->IsOper() || source == target;
			if (can_see_full)
			{
				if (cert->HasError())
					source->WriteNotice("Error: {}", cert->GetError());

				source->WriteNotice("*** Flags: {}known signer, {}revoked, {}trusted, {}valid",
					cert->IsKnownSigner() ? "" : "un", cert->IsRevoked() ? "" : "un",
					cert->IsTrusted() ? "" : "un", cert->IsValid() ? "" : "in");
			}

			if (!cert->GetDN().empty())
				source->WriteNotice("*** DN: {} ", cert->GetDN());

			if (!cert->GetIssuerDN().empty())
				source->WriteNotice("*** Issuer DN: {} ", cert->GetIssuerDN());

			if (can_see_full)
			{
				auto timestr = [](auto ts) {
					const auto tsdiff = ServerInstance->Time() - ts;
					return FMT::format("{} ({} {})",
						Time::ToString(ts, Time::DEFAULT_LONG),
						Duration::ToLongString(std::abs(tsdiff), true),
						tsdiff < ServerInstance->Time() ? "ago" : "from now"
					);
				};

				source->WriteNotice("*** Valid from: {}", timestr(cert->GetActivationTime()));
				source->WriteNotice("*** Valid until: {}", timestr(cert->GetExpirationTime()));
			}

			auto first = true;
			for (const auto& fingerprint : cert->GetFingerprints())
			{
				source->WriteNotice("*** Fingerprint{}: {}", first ? "" : " (fallback)", fingerprint);
				first = false;
			}
		}
	}

	CmdResult HandleUser(LocalUser* source, const std::string& nick)
	{
		auto* target = ServerInstance->Users.FindNick(nick, true);
		if (!target)
		{
			source->WriteNumeric(Numerics::NoSuchNick(nick));
			return CmdResult::FAILURE;
		}

		if (operonlyfp && !source->IsOper() && source != target)
		{
			source->WriteNumeric(Numerics::NoPrivileges("you must be a server operator to view TLS client certificate information for other users"));
			return CmdResult::FAILURE;
		}

		HandleUserInternal(source, target, true);
		return CmdResult::SUCCESS;
	}

	CmdResult HandleChannel(LocalUser* source, const std::string& channel)
	{
		auto* chan = ServerInstance->Channels.Find(channel);
		if (!chan)
		{
			source->WriteNumeric(Numerics::NoSuchChannel(channel));
			return CmdResult::FAILURE;
		}

		if (operonlyfp && !source->IsOper())
		{
			source->WriteNumeric(Numerics::NoPrivileges("you must be a server operator to view TLS client certificate information for channels"));
			return CmdResult::FAILURE;
		}

		if (!source->IsOper() && chan->GetPrefixValue(source) < OP_VALUE)
		{
			source->WriteNumeric(Numerics::ChannelPrivilegesNeeded(chan, OP_VALUE, "view TLS client certificate information"));
			return CmdResult::FAILURE;
		}

		if (sslonlymode)
		{
			source->WriteNotice("*** {} {} have channel mode +{} ({}) set.",
				chan->name, chan->IsModeSet(sslonlymode) ? "does" : "does not",
				sslonlymode->GetModeChar(), sslonlymode->service_name);
		}

		for (const auto& [u, _] : chan->GetUsers())
		{
			if (!u->server->IsService())
				HandleUserInternal(source, u, false);
		}

		return CmdResult::SUCCESS;
	}

public:
	TLSAPIImpl tlsapi;
	bool operonlyfp;

	CommandSSLInfo(const WeakModulePtr& Creator)
		: SplitCommand(Creator, "SSLINFO")
		, sslonlymode(Creator, "sslonly")
		, tlsapi(Creator)
	{
		syntax = { "[<channel|nick>]" };
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		if (parameters.empty())
		{
			HandleUserInternal(user, user, true);
			return CmdResult::SUCCESS;
		}

		if (ServerInstance->Channels.IsPrefix(parameters[0][0]))
			return HandleChannel(user, parameters[0]);

		return HandleUser(user, parameters[0]);
	}
};

class ModuleSSLInfo final
	: public Module
	, public Stats::EventListener
	, public WebIRC::EventListener
	, public Who::EventListener
	, public Whois::EventListener
{
private:
	struct WebIRCCertificate final
		: public TLS::Certificate
	{
		WebIRCCertificate(const WebIRC::FlagMap& flags, const std::vector<std::string>& algos)
		{
			this->known_signer = true;
			this->trusted = true;
			this->valid = true;

			for (const auto& algo : algos)
			{
				auto iter = flags.find(algo);
				if (iter != flags.end() && !iter->second.empty())
					this->fingerprints.push_back(iter->second);
			}

			if (this->GetFingerprints().empty())
				this->error = "WebIRC gateway did not send a client fingerprint";
		}
	};

	CommandSSLInfo cmd;
	std::vector<std::string> hashes;
	unsigned long warnexpiring;

	static bool MatchFingerprint(const TLS::Certificate* cert, const std::string& fp)
	{
		StringSplitter configfpstream(fp);
		for (std::string configfp; configfpstream.GetToken(configfp); )
		{
			for (const auto& certfp : cert->GetFingerprints())
			{
				if (InspIRCd::TimingSafeCompare(certfp, configfp))
					return true;
			}
		}

		return false;
	}

public:
	ModuleSSLInfo()
		: Module(VF_VENDOR, "Adds user facing TLS information, various TLS configuration options, and the /SSLINFO command to look up TLS certificate information for other users.")
		, Stats::EventListener(weak_from_this())
		, WebIRC::EventListener(weak_from_this())
		, Who::EventListener(weak_from_this())
		, Whois::EventListener(weak_from_this())
		, cmd(weak_from_this())
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("sslinfo");
		cmd.operonlyfp = tag->getBool("operonly");
		cmd.tlsapi.localsecure = tag->getBool("localsecure", true);
		warnexpiring = tag->getDuration("warnexpiring", 0, 0, 60*60*24*365);

		hashes.clear();
		StringSplitter hashstream(tag->getString("hash"));
		for (std::string hash; hashstream.GetToken(hash); )
		{
			if (!hash.compare(0, 5, "spki-", 5))
				hash.insert(4, "fp"); // spki-foo => spkifp-foo
			else
				hash.insert(0, "certfp-"); // foo => certfp-foo
			hashes.push_back(hash);
		}
	}

	void OnWhois(Whois::Context& whois) override
	{
		if (cmd.tlsapi.IsSecure(whois.GetTarget()))
			whois.SendLine(RPL_WHOISSECURE, "is using a secure connection");

		const auto* cert = cmd.tlsapi.GetCertificate(whois.GetTarget());
		if (cert && cert->IsUsable())
		{
			if (!cmd.operonlyfp || whois.IsSelfWhois() || whois.GetSource()->IsOper())
			{
				bool first = true;
				for (const auto& fingerprint : cert->GetFingerprints())
				{
					whois.SendLine(RPL_WHOISCERTFP, FMT::format("has {}client certificate fingerprint {}",
						first ? "" : "fallback ", fingerprint));
					first = false;
				}
			}
		}
	}

	ModResult OnWhoLine(const Who::Request& request, LocalUser* source, User* user, Membership* memb, Numeric::Numeric& numeric) override
	{
		size_t flag_index;
		if (!request.GetFieldIndex('f', flag_index))
			return MOD_RES_PASSTHRU;

		const auto* cert = cmd.tlsapi.GetCertificate(user);
		if (cert)
			numeric.GetParams()[flag_index].push_back('s');

		return MOD_RES_PASSTHRU;
	}

	ModResult OnPreOperLogin(LocalUser* user, const std::shared_ptr<OperAccount>& oper, bool automatic) override
	{
		auto* cert = cmd.tlsapi.GetCertificate(user);
		if (oper->GetConfig()->getBool("sslonly") && !cert)
		{
			if (!automatic)
			{
				ServerInstance->SNO.WriteGlobalSno('o', "{} ({}) [{}] failed to log into the \x02{}\x02 oper account because they are not connected using TLS.",
					user->nick, user->GetRealUserHost(), user->GetAddress(), oper->GetName());
			}
			return MOD_RES_DENY;
		}

		const auto fingerprint = oper->GetConfig()->getString("fingerprint");
		if (fingerprint.empty())
			return MOD_RES_PASSTHRU;

		const auto cert_usable = cert && cert->IsUsable();
		const auto correct_fp = cert_usable && MatchFingerprint(cert, fingerprint);
		if (!correct_fp)
		{
			if (!automatic)
			{
				const char* error;
				if (!cert)
					error = "not using a TLS client certificate";
				else if (!cert_usable)
					error = "using an invalid (probably expired) TLS client certificate";
				else
					error = "not using the correct TLS client certificate";

				ServerInstance->SNO.WriteGlobalSno('o', "{} ({}) [{}] failed to log into the \x02{}\x02 oper account because they are {}.",
					user->nick, user->GetRealUserHost(), user->GetAddress(), oper->GetName(), error);
			}
			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}

	void OnPostConnect(User* user) override
	{
		auto* const localuser = user->AsLocal();
		if (!localuser)
			return;

		auto* const cert = cmd.tlsapi.GetCertificate(localuser);
		if (!cert || !warnexpiring || !cert->GetExpirationTime())
			return;

		if (ServerInstance->Time() > cert->GetExpirationTime())
		{
			user->WriteNotice("*** Your TLS client certificate has expired.");
		}
		else if (static_cast<time_t>(ServerInstance->Time() + warnexpiring) > cert->GetExpirationTime())
		{
			const std::string duration = Duration::ToLongString(cert->GetExpirationTime() - ServerInstance->Time());
			user->WriteNotice("*** Your TLS client certificate expires in " + duration + ".");
		}
	}

	ModResult OnPreChangeConnectClass(LocalUser* user, const std::shared_ptr<ConnectClass>& klass, std::optional<Numeric::Numeric>& errnum) override
	{
		const auto* cert = cmd.tlsapi.GetCertificate(user);
		const char* error = nullptr;
		const std::string requiressl = klass->config->getString("requiressl");
		if (insp::equalsci(requiressl, "trusted"))
		{
			if (!cert || !cert->IsUsable(true))
				error = "a trusted TLS client certificate";
		}
		else if (klass->config->getBool("requiressl"))
		{
			if (!cert)
				error = "a TLS connection";
		}

		if (error)
		{
			ServerInstance->Logs.Debug("CONNECTCLASS", "The {} connect class is not suitable as it requires {}.",
				klass->GetName(), error);
			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnStats(Stats::Context& stats) override
	{
		if (stats.GetSymbol() != 't')
			return MOD_RES_PASSTHRU;

		// Counter for the number of users using each ciphersuite.
		std::map<std::string, size_t> counts;
		auto& plaintext = counts["Plain text"];
		auto& unknown = counts["Unknown"];
		for (auto* user : ServerInstance->Users.GetLocalUsers())
		{
			const auto* hook = TLS::GetHook(user->io->GetSocket());
			if (!hook)
			{
				plaintext++;
				continue;
			}

			std::string ciphersuite;
			if (!hook->GetCiphersuite(ciphersuite))
				unknown++;
			else
				counts[ciphersuite]++;
		}

		for (const auto& [ciphersuite, count] : counts)
		{
			if (!count)
				continue;

			stats.AddGenericRow(FMT::format("{}: {}", ciphersuite, count))
				.AddTags(stats, {
					{ "ciphersuite", ciphersuite      },
					{ "count",       ConvToStr(count) },
				});
		}
		return MOD_RES_DENY;
	}

	void OnWebIRCAuth(LocalUser* user, const WebIRC::FlagMap* flags) override
	{
		// We are only interested in connection flags. If none have been
		// given then we have nothing to do.
		if (!flags)
			return;

		// We only care about the tls connection flag if the connection
		// between the gateway and the server is secure.
		if (!cmd.tlsapi.GetCertificate(user))
			return;

		WebIRC::FlagMap::const_iterator iter = flags->find("secure");
		if (iter == flags->end())
		{
			// If this is not set then the connection between the client and
			// the gateway is not secure.
			cmd.tlsapi.notlsext.Set(user);
			cmd.tlsapi.tlsext.Unset(user);
			return;
		}

		// If the gateway specifies this flag we put all trust onto them for having validated the
		// client certificate. This is probably ill-advised but there's not much else we can do.
		cmd.tlsapi.SetCertificate(user, std::make_shared<WebIRCCertificate>(*flags, hashes));
	}

	void OnDecodeMetadata(Extensible* target, const std::string& extname, const std::string& extvalue) override
	{
		if (!target || target->extype != ExtensionType::USER || !insp::casemapped_equals(extname, "ssl_cert"))
			return; // Not for us

		if (extvalue[0] != 'c')
			return; // The remote also sent a tls-cert metadata so we can ignore this.

		cmd.tlsapi.SetCertificate(static_cast<User*>(target), std::make_shared<RemoteCertificate>(extvalue));
	}
};

MODULE_INIT(ModuleSSLInfo)
