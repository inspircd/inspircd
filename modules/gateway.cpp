/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2017-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014 md_5 <git@md-5.net>
 *   Copyright (C) 2014 Googolplexed <googol@googolplexed.net>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
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
#include "extension.h"
#include "modules/extban.h"
#include "modules/ssl.h"
#include "modules/webirc.h"
#include "modules/whois.h"

// One or more hostmask globs or CIDR ranges.
typedef std::vector<std::string> MaskList;

// Encapsulates information about a username gateway.
class UserHost final
{
private:
	MaskList hostmasks;
	std::string newuser;

public:
	UserHost(const MaskList& masks, const std::string& user)
		: hostmasks(masks)
		, newuser(user)
	{
	}

	const std::string& GetUser() const
	{
		return newuser;
	}

	bool Matches(LocalUser* user) const
	{
		for (const auto& mask : hostmasks)
		{
			// Does the user's hostname match this hostmask?
			if (InspIRCd::Match(user->GetRealHost(), mask, ascii_case_insensitive_map))
				return true;

			// Does the user's IP address match this hostmask?
			if (InspIRCd::MatchCIDR(user->GetAddress(), mask, ascii_case_insensitive_map))
				return true;
		}

		// The user didn't match any hostmasks.
		return false;
	}
};

// Encapsulates information about a WebIRC gateway.
class WebIRCHost final
{
private:
	MaskList hostmasks;
	std::string fingerprint;
	std::string password;
	std::string passhash;
	TokenList trustedflags;

public:
	WebIRCHost(const MaskList& masks, const std::string& fp, const std::string& pass, const std::string& hash, const std::string& flags)
		: hostmasks(masks)
		, fingerprint(fp)
		, password(pass)
		, passhash(hash)
		, trustedflags(flags)
	{
	}

	bool IsFlagTrusted(const std::string& flag) const
	{
		return trustedflags.Contains(flag);
	}

	bool Matches(LocalUser* user, const std::string& pass, UserCertificateAPI& sslapi) const
	{
		// Did the user send a valid password?
		if (!password.empty() && !InspIRCd::CheckPassword(password, passhash, pass))
			return false;

		// Does the user have a valid fingerprint?
		if (!fingerprint.empty())
		{
			if (!sslapi)
				return false;

			bool okay = false;
			for (const auto& fp : sslapi->GetFingerprints(user))
			{
				if (InspIRCd::TimingSafeCompare(fp, fingerprint))
				{
					okay = true;
					break;
				}
			}

			if (!okay)
				return false;
		}

		for (const auto& mask : hostmasks)
		{
			// Does the user's hostname match this hostmask?
			if (InspIRCd::Match(user->GetRealHost(), mask, ascii_case_insensitive_map))
				return true;

			// Does the user's IP address match this hostmask?
			if (InspIRCd::MatchCIDR(user->GetAddress(), mask, ascii_case_insensitive_map))
				return true;
		}

		// The user didn't match any hostmasks.
		return false;
	}
};

class CommandHexIP final
	: public SplitCommand
{
public:
	CommandHexIP(Module* Creator)
		: SplitCommand(Creator, "HEXIP", 1)
	{
		penalty = 2000;
		syntax = { "<hex-ip|raw-ip>" };
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		irc::sockets::sockaddrs sa(false);
		if (sa.from_ip(parameters[0]))
		{
			if (sa.family() != AF_INET)
			{
				user->WriteNotice("*** HEXIP: You can only hex encode an IPv4 address!");
				return CmdResult::FAILURE;
			}

			uint32_t addr = sa.in4.sin_addr.s_addr;
			user->WriteNotice(fmt::format("*** HEXIP: {} encodes to {:02x}{:02x}{:02x}{:02x}.",
				sa.addr(), (addr & 0xFF), ((addr >> 8) & 0xFF), ((addr >> 16) & 0xFF),
				((addr >> 24) & 0xFF)));
			return CmdResult::SUCCESS;
		}

		if (ParseIP(parameters[0], sa))
		{
			user->WriteNotice(fmt::format("*** HEXIP: {} decodes to {}.", parameters[0], sa.addr()));
			return CmdResult::SUCCESS;
		}

		user->WriteNotice(fmt::format("*** HEXIP: {} is not a valid raw or hex encoded IPv4 address.",
			parameters[0]));
		return CmdResult::FAILURE;
	}

	static bool ParseIP(const std::string& in, irc::sockets::sockaddrs& out)
	{
		const char* username = nullptr;
		if (in.length() == 8)
		{
			// The username is an IPv4 address encoded in hexadecimal with two characters
			// per address segment.
			username = in.c_str();
		}
		else if (in.length() == 9 && in[0] == '~')
		{
			// The same as above but m_ident got to this user before we did. Strip the
			// username prefix and continue as normal.
			username = in.c_str() + 1;
		}
		else
		{
			// The user either does not have an IPv4 in their username or the gateway server
			// is also running an identd. In the latter case there isn't really a lot we
			// can do so we just assume that the client in question is not connecting via
			// a username gateway.
			return false;
		}

		// Try to convert the IP address to a string. If this fails then the user
		// does not have an IPv4 address in their username.
		errno = 0;
		unsigned long address = strtoul(username, nullptr, 16);
		if (errno)
			return false;

		// If the converted IP address is > 32 bits then it's not valid so bail.
		if (address > UINT32_MAX)
			return false;

		out.in4.sin_family = AF_INET;
		out.in4.sin_addr.s_addr = htonl(uint32_t(address));
		return true;
	}
};

class GatewayExtBan final
	: public ExtBan::MatchingBase
{
public:
	StringExtItem gateway;

	GatewayExtBan(Module* Creator)
		: ExtBan::MatchingBase(Creator, "gateway", 'w')
		, gateway(Creator, "webirc-gateway", ExtensionType::USER, true)
	{
	}

	bool IsMatch(User* user, Channel* channel, const std::string& text) override
	{
		const std::string* gatewayname = gateway.Get(user);
		return gatewayname ? InspIRCd::Match(*gatewayname, text) : false;
	}
};

class CommandWebIRC final
	: public SplitCommand
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
		, realhost(Creator, "gateway-realhost", ExtensionType::USER, true)
		, realip(Creator, "gateway-realip", ExtensionType::USER, true)
		, sslapi(Creator)
		, webircevprov(Creator, "event/webirc")
	{
		works_before_reg = true;
		syntax = { "<password> <gateway> <hostname> <ip> [<flags>]" };
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		if (user->IsFullyConnected() || realhost.Get(user))
			return CmdResult::FAILURE;

		for (const auto& host : hosts)
		{
			// If we don't match the host then skip to the next host.
			if (!host.Matches(user, parameters[0], sslapi))
				continue;

			irc::sockets::sockaddrs ipaddr(false);
			if (!ipaddr.from_ip_port(parameters[3], user->client_sa.port()))
			{
				ServerInstance->SNO.WriteGlobalSno('w', "Connecting user {} ({}) tried to use WEBIRC but gave an invalid IP address.",
					user->uuid, user->GetAddress());
				ServerInstance->Users.QuitUser(user, "WEBIRC: IP address is invalid: " + parameters[3]);
				return CmdResult::FAILURE;
			}

			// The user matched a WebIRC block!
			extban.gateway.Set(user, parameters[1]);
			realhost.Set(user, user->GetRealHost());
			realip.Set(user, user->GetAddress());

			ServerInstance->SNO.WriteGlobalSno('w', "Connecting user {} is using the {} WebIRC gateway; changing their IP from {} to {}.",
				user->uuid, parameters[1], user->GetAddress(), parameters[3]);

			// If we have custom flags then deal with them.
			WebIRC::FlagMap flags;
			const bool hasflags = (parameters.size() > 4);
			if (hasflags)
			{
				std::string flagname;
				std::string flagvalue;

				// Parse the flags.
				irc::spacesepstream flagstream(parameters[4]);
				for (std::string flag; flagstream.GetToken(flag); )
				{
					// Does this flag have a value?
					const size_t separator = flag.find('=');
					if (separator == std::string::npos)
					{
						// It does not; just use the flag.
						flagname = flag;
						flagvalue.clear();
					}
					else
					{
						// It does; extract the value.
						flagname = flag.substr(0, separator);
						flagvalue = flag.substr(separator + 1);
					}

					if (host.IsFlagTrusted(flagname))
						flags[flagname] = flagvalue;
				}
			}

			// Inform modules about the WebIRC attempt.
			webircevprov.Call(&WebIRC::EventListener::OnWebIRCAuth, user, (hasflags ? &flags : nullptr));

			// Set the IP address sent via WEBIRC. We ignore the hostname and lookup
			// instead do our own DNS lookups because of unreliable gateways.
			user->ChangeRemoteAddress(ipaddr);
			return CmdResult::SUCCESS;
		}

		ServerInstance->SNO.WriteGlobalSno('w', "Connecting user {} ({}) tried to use WEBIRC but didn't match any configured WebIRC hosts.",
			user->uuid, user->GetAddress());
		ServerInstance->Users.QuitUser(user, "WEBIRC: you don't match any configured WebIRC hosts.");
		return CmdResult::FAILURE;
	}
};

class ModuleGateway final
	: public Module
	, public WebIRC::EventListener
	, public Whois::EventListener
{
private:
	CommandHexIP cmdhexip;
	CommandWebIRC cmdwebirc;
	std::vector<UserHost> hosts;

public:
	ModuleGateway()
		: Module(VF_VENDOR, "Adds the ability for IRC gateways to forward the real IP address of users connecting through them.")
		, WebIRC::EventListener(this)
		, Whois::EventListener(this)
		, cmdhexip(this)
		, cmdwebirc(this)
	{
	}

	void init() override
	{
		ServerInstance->SNO.EnableSnomask('w', "GATEWAY");
	}

	void ReadConfig(ConfigStatus& status) override
	{
		std::vector<UserHost> userhosts;
		std::vector<WebIRCHost> webirchosts;

		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("gateway", ServerInstance->Config->ConfTags("cgihost")))
		{
			MaskList masks;
			irc::spacesepstream maskstream(tag->getString("mask"));
			for (std::string mask; maskstream.GetToken(mask); )
				masks.push_back(mask);

			// Ensure that we have the <gateway:mask> parameter.
			if (masks.empty())
				throw ModuleException(this, "<" + tag->name + ":mask> is a mandatory field, at " + tag->source.str());

			// Determine what lookup type this host uses.
			const std::string type = tag->getString("type");
			if (insp::equalsci(type, "username") || insp::equalsci(type, "ident"))
			{
				// The IP address should be looked up from the hex IP address.
				const std::string newuser = tag->getString("newusername", "gateway", ServerInstance->IsUser);
				userhosts.emplace_back(masks, newuser);
			}
			else if (insp::equalsci(type, "webirc"))
			{
				// The IP address will be received via the WEBIRC command.
				const std::string fingerprint = tag->getString("fingerprint");
				const std::string password = tag->getString("password");
				const std::string passwordhash = tag->getString("hash", "plaintext", 1);
				const std::string trustedflags = tag->getString("trustedflags", "*", 1);

				// WebIRC blocks require a password.
				if (fingerprint.empty() && password.empty())
					throw ModuleException(this, "When using <" + tag->name + " type=\"webirc\"> either the fingerprint or password field is required, at " + tag->source.str());

				if (!password.empty() && insp::equalsci(passwordhash, "plaintext"))
				{
					ServerInstance->Logs.Normal(MODNAME, "<{}> tag at {} contains an plain text password, this is insecure!",
						tag->name, tag->source.str());
				}

				webirchosts.emplace_back(masks, fingerprint, password, passwordhash, trustedflags);
			}
			else
			{
				throw ModuleException(this, type + " is an invalid <" + tag->name + ":mask> type, at " + tag->source.str());
			}
		}

		// The host configuration was valid so we can apply it.
		hosts.swap(userhosts);
		cmdwebirc.hosts.swap(webirchosts);
	}

	ModResult OnPreChangeConnectClass(LocalUser* user, const std::shared_ptr<ConnectClass>& klass, std::optional<Numeric::Numeric>& errnum) override
	{
		// If <connect:webirc> is not set then we have nothing to do.
		const std::string webirc = klass->config->getString("webirc");
		if (webirc.empty())
			return MOD_RES_PASSTHRU;

		// If the user is not connecting via a WebIRC gateway then they
		// cannot match this connect class.
		const std::string* gateway = cmdwebirc.extban.gateway.Get(user);
		if (!gateway)
		{
			ServerInstance->Logs.Debug("CONNECTCLASS", "The {} connect class is not suitable as it requires a connection via a WebIRC gateway.",
				klass->GetName());
			return MOD_RES_DENY;
		}

		// If the gateway matches the <connect:webirc> constraint then
		// allow the check to continue. Otherwise, reject it.
		if (!InspIRCd::Match(*gateway, webirc))
		{
			ServerInstance->Logs.Debug("CONNECTCLASS", "The {} connect class is not suitable as the WebIRC gateway name ({}) does not match {}.",
				klass->GetName(), *gateway, webirc);
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

			// We have matched an gateway block! Try to parse the encoded IPv4 address
			// out of the user.
			irc::sockets::sockaddrs address(user->client_sa);
			if (!CommandHexIP::ParseIP(user->GetRealUser(), address))
				return MOD_RES_PASSTHRU;

			// Store the hostname and IP of the gateway for later use.
			cmdwebirc.realhost.Set(user, user->GetRealHost());
			cmdwebirc.realip.Set(user, user->GetAddress());

			const std::string& newuser = host.GetUser();
			ServerInstance->SNO.WriteGlobalSno('w', "Connecting user {} is using a username gateway; changing their IP from {} to {} and their real username from {} to {}.",
				user->uuid, user->GetAddress(), address.addr(), user->GetRealUser(), newuser);

			user->ChangeRealUser(newuser, user->GetDisplayedUser() == user->GetRealUser());
			user->ChangeRemoteAddress(address);
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
			in_port_t port = ConvToNum<in_port_t>(cport->second);
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
						ServerInstance->Logs.Debug(MODNAME, "BUG: OnWebIRCAuth({}): socket type {} is unknown!",
							user->uuid, user->client_sa.family());
						return;
				}
			}
		}

		WebIRC::FlagMap::const_iterator sport = flags->find("local-port");
		if (sport != flags->end())
		{
			// If we can't parse the port then just give up.
			in_port_t port = ConvToNum<in_port_t>(sport->second);
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
						ServerInstance->Logs.Debug(MODNAME, "BUG: OnWebIRCAuth({}): socket type {} is unknown!",
							user->uuid, user->server_sa.family());
						return;
				}
			}
		}
	}

	void OnWhois(Whois::Context& whois) override
	{
		// If these fields are not set then the client is not using a gateway.
		std::string* realhost = cmdwebirc.realhost.Get(whois.GetTarget());
		std::string* realip = cmdwebirc.realip.Get(whois.GetTarget());
		if (!realhost || !realip)
			return;

		// If the source doesn't have the right privs then only show the gateway name.
		std::string hidden = "*";
		if (!whois.GetSource()->HasPrivPermission("users/auspex"))
			realhost = realip = &hidden;

		const std::string* gateway = cmdwebirc.extban.gateway.Get(whois.GetTarget());
		if (gateway)
			whois.SendLine(RPL_WHOISGATEWAY, *realhost, *realip, "is connected via the " + *gateway + " WebIRC gateway");
		else
			whois.SendLine(RPL_WHOISGATEWAY, *realhost, *realip, "is connected via a username gateway");
	}
};

MODULE_INIT(ModuleGateway)
