/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2014 md_5 <git@md-5.net>
 *   Copyright (C) 2014 Googolplexed <googol@googolplexed.net>
 *   Copyright (C) 2013, 2017-2018, 2020-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012-2013, 2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2007-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007, 2010 Craig Edwards <brain@inspircd.org>
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
#include "modules/extban.h"
#include "modules/ssl.h"
#include "modules/webirc.h"
#include "modules/whois.h"

enum
{
	// InspIRCd-specific.
	RPL_WHOISGATEWAY = 350
};

// One or more hostmask globs or CIDR ranges.
typedef std::vector<std::string> MaskList;

// Encapsulates information about an ident host.
class IdentHost
{
 private:
	MaskList hostmasks;
	std::string newident;

 public:
	IdentHost(const MaskList& masks, const std::string& ident)
		: hostmasks(masks)
		, newident(ident)
	{
	}

	const std::string& GetIdent() const
	{
		return newident;
	}

	bool Matches(LocalUser* user) const
	{
		for (const auto& mask : hostmasks)
		{
			// Does the user's hostname match this hostmask?
			if (InspIRCd::Match(user->GetRealHost(), mask, ascii_case_insensitive_map))
				return true;

			// Does the user's IP address match this hostmask?
			if (InspIRCd::MatchCIDR(user->GetIPString(), mask, ascii_case_insensitive_map))
				return true;
		}

		// The user didn't match any hostmasks.
		return false;
	}
};

// Encapsulates information about a WebIRC host.
class WebIRCHost
{
 private:
	MaskList hostmasks;
	std::string fingerprint;
	std::string password;
	std::string passhash;

 public:
	WebIRCHost(const MaskList& masks, const std::string& fp, const std::string& pass, const std::string& hash)
		: hostmasks(masks)
		, fingerprint(fp)
		, password(pass)
		, passhash(hash)
	{
	}

	bool Matches(LocalUser* user, const std::string& pass, UserCertificateAPI& sslapi) const
	{
		// Did the user send a valid password?
		if (!password.empty() && !ServerInstance->PassCompare(user, password, pass, passhash))
			return false;

		// Does the user have a valid fingerprint?
		const std::string fp = sslapi ? sslapi->GetFingerprint(user) : "";
		if (!fingerprint.empty() && !InspIRCd::TimingSafeCompare(fp, fingerprint))
			return false;

		for (const auto& mask : hostmasks)
		{
			// Does the user's hostname match this hostmask?
			if (InspIRCd::Match(user->GetRealHost(), mask, ascii_case_insensitive_map))
				return true;

			// Does the user's IP address match this hostmask?
			if (InspIRCd::MatchCIDR(user->GetIPString(), mask, ascii_case_insensitive_map))
				return true;
		}

		// The user didn't match any hostmasks.
		return false;
	}
};

class CommandHexIP : public SplitCommand
{
 public:
	CommandHexIP(Module* Creator)
		: SplitCommand(Creator, "HEXIP", 1)
	{
		allow_empty_last_param = false;
		Penalty = 2;
		syntax = { "<hex-ip|raw-ip>" };
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		irc::sockets::sockaddrs sa;
		if (irc::sockets::aptosa(parameters[0], 0, sa))
		{
			if (sa.family() != AF_INET)
			{
				user->WriteNotice("*** HEXIP: You can only hex encode an IPv4 address!");
				return CmdResult::FAILURE;
			}

			uint32_t addr = sa.in4.sin_addr.s_addr;
			user->WriteNotice(InspIRCd::Format("*** HEXIP: %s encodes to %02x%02x%02x%02x.",
				sa.addr().c_str(), (addr & 0xFF), ((addr >> 8) & 0xFF), ((addr >> 16) & 0xFF),
				((addr >> 24) & 0xFF)));
			return CmdResult::SUCCESS;
		}

		if (ParseIP(parameters[0], sa))
		{
			user->WriteNotice(InspIRCd::Format("*** HEXIP: %s decodes to %s.",
				parameters[0].c_str(), sa.addr().c_str()));
			return CmdResult::SUCCESS;
		}

		user->WriteNotice(InspIRCd::Format("*** HEXIP: %s is not a valid raw or hex encoded IPv4 address.",
			parameters[0].c_str()));
		return CmdResult::FAILURE;
	}

	static bool ParseIP(const std::string& in, irc::sockets::sockaddrs& out)
	{
		const char* ident = NULL;
		if (in.length() == 8)
		{
			// The ident is an IPv4 address encoded in hexadecimal with two characters
			// per address segment.
			ident = in.c_str();
		}
		else if (in.length() == 9 && in[0] == '~')
		{
			// The same as above but m_ident got to this user before we did. Strip the
			// ident prefix and continue as normal.
			ident = in.c_str() + 1;
		}
		else
		{
			// The user either does not have an IPv4 in their ident or the gateway server
			// is also running an identd. In the latter case there isn't really a lot we
			// can do so we just assume that the client in question is not connecting via
			// an ident gateway.
			return false;
		}

		// Try to convert the IP address to a string. If this fails then the user
		// does not have an IPv4 address in their ident.
		errno = 0;
		unsigned long address = strtoul(ident, NULL, 16);
		if (errno)
			return false;

		out.in4.sin_family = AF_INET;
		out.in4.sin_addr.s_addr = htonl(address);
		return true;
	}
};

class GatewayExtBan
	: public ExtBan::MatchingBase
{
 public:
	StringExtItem gateway;

	GatewayExtBan(Module* Creator)
		: ExtBan::MatchingBase(Creator, "gateway", 'w')
		, gateway(Creator, "cgiirc_gateway", ExtensionItem::EXT_USER, true)
	{
	}

	bool IsMatch(User* user, Channel* channel, const std::string& text) override
	{
		const std::string* gatewayname = gateway.Get(user);
		return gatewayname ? InspIRCd::Match(*gatewayname, text) : false;
	}
};

class CommandWebIRC : public SplitCommand
{
 public:
	std::vector<WebIRCHost> hosts;
	GatewayExtBan extban;
	StringExtItem realhost;
	StringExtItem realip;
	UserCertificateAPI sslapi;
	Events::ModuleEventProvider webircevprov;

	CommandWebIRC(Module* Creator)
		: SplitCommand(Creator, "WEBIRC", 4)
		, extban(Creator)
		, realhost(Creator, "cgiirc_realhost", ExtensionItem::EXT_USER, true)
		, realip(Creator, "cgiirc_realip", ExtensionItem::EXT_USER, true)
		, sslapi(Creator)
		, webircevprov(Creator, "event/webirc")
	{
		allow_empty_last_param = false;
		works_before_reg = true;
		syntax = { "<password> <gateway> <hostname> <ip> [<flags>]" };
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		if (user->registered == REG_ALL || realhost.Get(user))
			return CmdResult::FAILURE;

		for (const auto& host : hosts)
		{
			// If we don't match the host then skip to the next host.
			if (!host.Matches(user, parameters[0], sslapi))
				continue;

			irc::sockets::sockaddrs ipaddr;
			if (!irc::sockets::aptosa(parameters[3], user->client_sa.port(), ipaddr))
			{
				ServerInstance->SNO.WriteGlobalSno('w', "Connecting user %s (%s) tried to use WEBIRC but gave an invalid IP address.",
					user->uuid.c_str(), user->GetIPString().c_str());
				ServerInstance->Users.QuitUser(user, "WEBIRC: IP address is invalid: " + parameters[3]);
				return CmdResult::FAILURE;
			}

			// The user matched a WebIRC block!
			extban.gateway.Set(user, parameters[1]);
			realhost.Set(user, user->GetRealHost());
			realip.Set(user, user->GetIPString());

			ServerInstance->SNO.WriteGlobalSno('w', "Connecting user %s is using the %s WebIRC gateway; changing their IP from %s to %s.",
				user->uuid.c_str(), parameters[1].c_str(),
				user->GetIPString().c_str(), parameters[3].c_str());

			// If we have custom flags then deal with them.
			WebIRC::FlagMap flags;
			const bool hasflags = (parameters.size() > 4);
			if (hasflags)
			{
				// Parse the flags.
				irc::spacesepstream flagstream(parameters[4]);
				for (std::string flag; flagstream.GetToken(flag); )
				{
					// Does this flag have a value?
					const size_t separator = flag.find('=');
					if (separator == std::string::npos)
					{
						flags[flag];
						continue;
					}

					// The flag has a value!
					const std::string key = flag.substr(0, separator);
					const std::string value = flag.substr(separator + 1);
					flags[key] = value;
				}
			}

			// Inform modules about the WebIRC attempt.
			webircevprov.Call(&WebIRC::EventListener::OnWebIRCAuth, user, (hasflags ? &flags : nullptr));

			// Set the IP address sent via WEBIRC. We ignore the hostname and lookup
			// instead do our own DNS lookups because of unreliable gateways.
			user->SetClientIP(ipaddr);
			return CmdResult::SUCCESS;
		}

		ServerInstance->SNO.WriteGlobalSno('w', "Connecting user %s (%s) tried to use WEBIRC but didn't match any configured WebIRC hosts.",
			user->uuid.c_str(), user->GetIPString().c_str());
		ServerInstance->Users.QuitUser(user, "WEBIRC: you don't match any configured WebIRC hosts.");
		return CmdResult::FAILURE;
	}
};

class ModuleCgiIRC
	: public Module
	, public WebIRC::EventListener
	, public Whois::EventListener
{
 private:
	CommandHexIP cmdhexip;
	CommandWebIRC cmdwebirc;
	std::vector<IdentHost> hosts;

 public:
	ModuleCgiIRC()
		: Module(VF_VENDOR, "Adds the ability for IRC gateways to forward the real IP address of users connecting through them.")
		, WebIRC::EventListener(this)
		, Whois::EventListener(this)
		, cmdhexip(this)
		, cmdwebirc(this)
	{
	}

	void init() override
	{
		ServerInstance->SNO.EnableSnomask('w', "CGIIRC");
	}

	void ReadConfig(ConfigStatus& status) override
	{
		std::vector<IdentHost> identhosts;
		std::vector<WebIRCHost> webirchosts;

		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("cgihost"))
		{
			MaskList masks;
			irc::spacesepstream maskstream(tag->getString("mask"));
			for (std::string mask; maskstream.GetToken(mask); )
				masks.push_back(mask);

			// Ensure that we have the <cgihost:mask> parameter.
			if (masks.empty())
				throw ModuleException("<cgihost:mask> is a mandatory field, at " + tag->source.str());

			// Determine what lookup type this host uses.
			const std::string type = tag->getString("type");
			if (stdalgo::string::equalsci(type, "ident"))
			{
				// The IP address should be looked up from the hex IP address.
				const std::string newident = tag->getString("newident", "gateway", ServerInstance->IsIdent);
				identhosts.emplace_back(masks, newident);
			}
			else if (stdalgo::string::equalsci(type, "webirc"))
			{
				// The IP address will be received via the WEBIRC command.
				const std::string fingerprint = tag->getString("fingerprint");
				const std::string password = tag->getString("password");
				const std::string passwordhash = tag->getString("hash", "plaintext", 1);

				// WebIRC blocks require a password.
				if (fingerprint.empty() && password.empty())
					throw ModuleException("When using <cgihost type=\"webirc\"> either the fingerprint or password field is required, at " + tag->source.str());

				if (!password.empty() && stdalgo::string::equalsci(passwordhash, "plaintext"))
				{
					ServerInstance->Logs.Log(MODNAME, LOG_DEFAULT, "<cgihost> tag at %s contains an plain text password, this is insecure!",
						tag->source.str().c_str());
				}

				webirchosts.emplace_back(masks, fingerprint, password, passwordhash);
			}
			else
			{
				throw ModuleException(type + " is an invalid <cgihost:mask> type, at " + tag->source.str());
			}
		}

		// The host configuration was valid so we can apply it.
		hosts.swap(identhosts);
		cmdwebirc.hosts.swap(webirchosts);
	}

	ModResult OnSetConnectClass(LocalUser* user, std::shared_ptr<ConnectClass> myclass) override
	{
		// If <connect:webirc> is not set then we have nothing to do.
		const std::string webirc = myclass->config->getString("webirc");
		if (webirc.empty())
			return MOD_RES_PASSTHRU;

		// If the user is not connecting via a WebIRC gateway then they
		// cannot match this connect class.
		const std::string* gateway = cmdwebirc.extban.gateway.Get(user);
		if (!gateway)
		{
			ServerInstance->Logs.Log("CONNECTCLASS", LOG_DEBUG, "The %s connect class is not suitable as it requires a connection via a WebIRC gateway",
					myclass->GetName().c_str());
			return MOD_RES_DENY;
		}

		// If the gateway matches the <connect:webirc> constraint then
		// allow the check to continue. Otherwise, reject it.
		if (!InspIRCd::Match(*gateway, webirc))
		{
			ServerInstance->Logs.Log("CONNECTCLASS", LOG_DEBUG, "The %s connect class is not suitable as the WebIRC gateway name (%s) does not match %s",
					myclass->GetName().c_str(), gateway->c_str(), webirc.c_str());
			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserRegister(LocalUser* user) override
	{
		// There is no need to check for gateways if one is already being used.
		if (cmdwebirc.realhost.Get(user))
			return MOD_RES_PASSTHRU;

		for (const auto& host : hosts)
		{
			// If we don't match the host then skip to the next host.
			if (!host.Matches(user))
				continue;

			// We have matched an <cgihost> block! Try to parse the encoded IPv4 address
			// out of the ident.
			irc::sockets::sockaddrs address(user->client_sa);
			if (!CommandHexIP::ParseIP(user->ident, address))
				return MOD_RES_PASSTHRU;

			// Store the hostname and IP of the gateway for later use.
			cmdwebirc.realhost.Set(user, user->GetRealHost());
			cmdwebirc.realip.Set(user, user->GetIPString());

			const std::string& newident = host.GetIdent();
			ServerInstance->SNO.WriteGlobalSno('w', "Connecting user %s is using an ident gateway; changing their IP from %s to %s and their ident from %s to %s.",
				user->uuid.c_str(), user->GetIPString().c_str(), address.addr().c_str(), user->ident.c_str(), newident.c_str());

			user->ChangeIdent(newident);
			user->SetClientIP(address);
			break;
		}
		return MOD_RES_PASSTHRU;
	}

	void OnWebIRCAuth(LocalUser* user, const WebIRC::FlagMap* flags) override
	{
		// We are only interested in connection flags. If none have been
		// given then we have nothing to do.
		if (!flags)
			return;

		WebIRC::FlagMap::const_iterator cport = flags->find("remote-port");
		if (cport != flags->end())
		{
			// If we can't parse the port then just give up.
			uint16_t port = ConvToNum<uint16_t>(cport->second);
			if (port)
			{
				switch (user->client_sa.family())
				{
					case AF_INET:
						user->client_sa.in4.sin_port = htons(port);
						break;

					case AF_INET6:
						user->client_sa.in6.sin6_port = htons(port);
						break;

					default:
						// If we have reached this point then we have encountered a bug.
						ServerInstance->Logs.Log(MODNAME, LOG_DEBUG, "BUG: OnWebIRCAuth(%s): socket type %d is unknown!",
							user->uuid.c_str(), user->client_sa.family());
						return;
				}
			}
		}

		WebIRC::FlagMap::const_iterator sport = flags->find("local-port");
		if (sport != flags->end())
		{
			// If we can't parse the port then just give up.
			uint16_t port = ConvToNum<uint16_t>(sport->second);
			if (port)
			{
				switch (user->server_sa.family())
				{
					case AF_INET:
						user->server_sa.in4.sin_port = htons(port);
						break;

					case AF_INET6:
						user->server_sa.in6.sin6_port = htons(port);
						break;

					default:
						// If we have reached this point then we have encountered a bug.
						ServerInstance->Logs.Log(MODNAME, LOG_DEBUG, "BUG: OnWebIRCAuth(%s): socket type %d is unknown!",
							user->uuid.c_str(), user->server_sa.family());
						return;
				}
			}
		}
	}

	void OnWhois(Whois::Context& whois) override
	{
		// If these fields are not set then the client is not using a gateway.
		const std::string* realhost = cmdwebirc.realhost.Get(whois.GetTarget());
		const std::string* realip = cmdwebirc.realip.Get(whois.GetTarget());
		if (!realhost || !realip)
			return;

		const std::string* gateway = cmdwebirc.extban.gateway.Get(whois.GetTarget());
		if (gateway)
			whois.SendLine(RPL_WHOISGATEWAY, *realhost, *realip, "is connected via the " + *gateway + " WebIRC gateway");
		else
			whois.SendLine(RPL_WHOISGATEWAY, *realhost, *realip, "is connected via an ident gateway");
	}
};

MODULE_INIT(ModuleCgiIRC)
