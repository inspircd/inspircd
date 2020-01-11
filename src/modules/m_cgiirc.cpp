/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2014 md_5 <git@md-5.net>
 *   Copyright (C) 2014 Googolplexed <googol@googolplexed.net>
 *   Copyright (C) 2013, 2017-2018 Sadie Powell <sadie@witchery.services>
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
#include "modules/ssl.h"
#include "modules/webirc.h"
#include "modules/whois.h"

enum
{
	// InspIRCd-specific.
	RPL_WHOISGATEWAY = 350
};

// Encapsulates information about an ident host.
class IdentHost
{
 private:
	std::string hostmask;
	std::string newident;

 public:
	IdentHost(const std::string& mask, const std::string& ident)
		: hostmask(mask)
		, newident(ident)
	{
	}

	const std::string& GetIdent() const
	{
		return newident;
	}

	bool Matches(LocalUser* user) const
	{
		if (!InspIRCd::Match(user->GetRealHost(), hostmask, ascii_case_insensitive_map))
			return false;

		return InspIRCd::MatchCIDR(user->GetIPString(), hostmask, ascii_case_insensitive_map);
	}
};

// Encapsulates information about a WebIRC host.
class WebIRCHost
{
 private:
	std::string hostmask;
	std::string fingerprint;
	std::string password;
	std::string passhash;

 public:
	WebIRCHost(const std::string& mask, const std::string& fp, const std::string& pass, const std::string& hash)
		: hostmask(mask)
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

		// Does the user's hostname match our hostmask?
		if (InspIRCd::Match(user->GetRealHost(), hostmask, ascii_case_insensitive_map))
			return true;

		// Does the user's IP address match our hostmask?
		return InspIRCd::MatchCIDR(user->GetIPString(), hostmask, ascii_case_insensitive_map);
	}
};

/*
 * WEBIRC
 *  This is used for the webirc method of CGIIRC auth, and is (really) the best way to do these things.
 *  Syntax: WEBIRC password gateway hostname ip
 *  Where password is a shared key, gateway is the name of the WebIRC gateway and version (e.g. cgiirc), hostname
 *  is the resolved host of the client issuing the command and IP is the real IP of the client.
 *
 * How it works:
 *  To tie in with the rest of cgiirc module, and to avoid race conditions, /webirc is only processed locally
 *  and simply sets metadata on the user, which is later decoded on full connect to give something meaningful.
 */
class CommandWebIRC : public SplitCommand
{
 public:
	std::vector<WebIRCHost> hosts;
	bool notify;
	StringExtItem gateway;
	StringExtItem realhost;
	StringExtItem realip;
	UserCertificateAPI sslapi;
	Events::ModuleEventProvider webircevprov;

	CommandWebIRC(Module* Creator)
		: SplitCommand(Creator, "WEBIRC", 4)
		, gateway("cgiirc_gateway", ExtensionItem::EXT_USER, Creator)
		, realhost("cgiirc_realhost", ExtensionItem::EXT_USER, Creator)
		, realip("cgiirc_realip", ExtensionItem::EXT_USER, Creator)
		, sslapi(Creator)
		, webircevprov(Creator, "event/webirc")
	{
		allow_empty_last_param = false;
		works_before_reg = true;
		this->syntax = "<password> <gateway> <hostname> <ip> [<flags>]";
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) CXX11_OVERRIDE
	{
		if (user->registered == REG_ALL || realhost.get(user))
			return CMD_FAILURE;

		for (std::vector<WebIRCHost>::const_iterator iter = hosts.begin(); iter != hosts.end(); ++iter)
		{
			// If we don't match the host then skip to the next host.
			if (!iter->Matches(user, parameters[0], sslapi))
				continue;

			irc::sockets::sockaddrs ipaddr;
			if (!irc::sockets::aptosa(parameters[3], user->client_sa.port(), ipaddr))
			{
				WriteLog("Connecting user %s (%s) tried to use WEBIRC but gave an invalid IP address.",
					user->uuid.c_str(), user->GetIPString().c_str());
				ServerInstance->Users->QuitUser(user, "WEBIRC: IP address is invalid: " + parameters[3]);
				return CMD_FAILURE;
			}

			// The user matched a WebIRC block!
			gateway.set(user, parameters[1]);
			realhost.set(user, user->GetRealHost());
			realip.set(user, user->GetIPString());

			WriteLog("Connecting user %s is using a WebIRC gateway; changing their IP from %s to %s.",
				user->uuid.c_str(), user->GetIPString().c_str(), parameters[3].c_str());

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
			FOREACH_MOD_CUSTOM(webircevprov, WebIRC::EventListener, OnWebIRCAuth, (user, (hasflags ? &flags : NULL)));

			// Set the IP address sent via WEBIRC. We ignore the hostname and lookup
			// instead do our own DNS lookups because of unreliable gateways.
			user->SetClientIP(ipaddr);
			return CMD_SUCCESS;
		}

		WriteLog("Connecting user %s (%s) tried to use WEBIRC but didn't match any configured WebIRC hosts.",
			user->uuid.c_str(), user->GetIPString().c_str());
		ServerInstance->Users->QuitUser(user, "WEBIRC: you don't match any configured WebIRC hosts.");
		return CMD_FAILURE;
	}

	void WriteLog(const char* message, ...) CUSTOM_PRINTF(2, 3)
	{
		std::string buffer;
		VAFORMAT(buffer, message, message);

		// If we are sending a snotice then the message will already be
		// written to the logfile.
		if (notify)
			ServerInstance->SNO->WriteGlobalSno('w', buffer);
		else
			ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, buffer);
	}
};

class ModuleCgiIRC
	: public Module
	, public WebIRC::EventListener
	, public Whois::EventListener
{
 private:
	CommandWebIRC cmd;
	std::vector<IdentHost> hosts;

	static bool ParseIdent(LocalUser* user, irc::sockets::sockaddrs& out)
	{
		const char* ident = NULL;
		if (user->ident.length() == 8)
		{
			// The ident is an IPv4 address encoded in hexadecimal with two characters
			// per address segment.
			ident = user->ident.c_str();
		}
		else if (user->ident.length() == 9 && user->ident[0] == '~')
		{
			// The same as above but m_ident got to this user before we did. Strip the
			// ident prefix and continue as normal.
			ident = user->ident.c_str() + 1;
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

 public:
	ModuleCgiIRC()
		: WebIRC::EventListener(this)
		, Whois::EventListener(this)
		, cmd(this)
	{
	}

	void init() CXX11_OVERRIDE
	{
		ServerInstance->SNO->EnableSnomask('w', "CGIIRC");
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		std::vector<IdentHost> identhosts;
		std::vector<WebIRCHost> webirchosts;

		ConfigTagList tags = ServerInstance->Config->ConfTags("cgihost");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			ConfigTag* tag = i->second;

			// Ensure that we have the <cgihost:mask> parameter.
			const std::string mask = tag->getString("mask");
			if (mask.empty())
				throw ModuleException("<cgihost:mask> is a mandatory field, at " + tag->getTagLocation());

			// Determine what lookup type this host uses.
			const std::string type = tag->getString("type");
			if (stdalgo::string::equalsci(type, "ident"))
			{
				// The IP address should be looked up from the hex IP address.
				const std::string newident = tag->getString("newident", "gateway", ServerInstance->IsIdent);
				identhosts.push_back(IdentHost(mask, newident));
			}
			else if (stdalgo::string::equalsci(type, "webirc"))
			{
				// The IP address will be received via the WEBIRC command.
				const std::string fingerprint = tag->getString("fingerprint");
				const std::string password = tag->getString("password");

				// WebIRC blocks require a password.
				if (fingerprint.empty() && password.empty())
					throw ModuleException("When using <cgihost type=\"webirc\"> either the fingerprint or password field is required, at " + tag->getTagLocation());

				webirchosts.push_back(WebIRCHost(mask, fingerprint, password, tag->getString("hash")));
			}
			else
			{
				throw ModuleException(type + " is an invalid <cgihost:mask> type, at " + tag->getTagLocation());
			}
		}

		// The host configuration was valid so we can apply it.
		hosts.swap(identhosts);
		cmd.hosts.swap(webirchosts);

		// Do we send an oper notice when a m_cgiirc client has their IP changed?
		cmd.notify = ServerInstance->Config->ConfValue("cgiirc")->getBool("opernotice", true);
	}

	ModResult OnSetConnectClass(LocalUser* user, ConnectClass* myclass) CXX11_OVERRIDE
	{
		// If <connect:webirc> is not set then we have nothing to do.
		const std::string webirc = myclass->config->getString("webirc");
		if (webirc.empty())
			return MOD_RES_PASSTHRU;

		// If the user is not connecting via a WebIRC gateway then they
		// cannot match this connect class.
		const std::string* gateway = cmd.gateway.get(user);
		if (!gateway)
			return MOD_RES_DENY;

		// If the gateway matches the <connect:webirc> constraint then
		// allow the check to continue. Otherwise, reject it.
		return InspIRCd::Match(*gateway, webirc) ? MOD_RES_PASSTHRU : MOD_RES_DENY;
	}

	ModResult OnUserRegister(LocalUser* user) CXX11_OVERRIDE
	{
		// There is no need to check for gateways if one is already being used.
		if (cmd.realhost.get(user))
			return MOD_RES_PASSTHRU;

		for (std::vector<IdentHost>::const_iterator iter = hosts.begin(); iter != hosts.end(); ++iter)
		{
			// If we don't match the host then skip to the next host.
			if (!iter->Matches(user))
				continue;

			// We have matched an <cgihost> block! Try to parse the encoded IPv4 address
			// out of the ident.
			irc::sockets::sockaddrs address(user->client_sa);
			if (!ParseIdent(user, address))
				return MOD_RES_PASSTHRU;

			// Store the hostname and IP of the gateway for later use.
			cmd.realhost.set(user, user->GetRealHost());
			cmd.realip.set(user, user->GetIPString());

			const std::string& newident = iter->GetIdent();
			cmd.WriteLog("Connecting user %s is using an ident gateway; changing their IP from %s to %s and their ident from %s to %s.",
				user->uuid.c_str(), user->GetIPString().c_str(), address.addr().c_str(), user->ident.c_str(), newident.c_str());

			user->ChangeIdent(newident);
			user->SetClientIP(address);
			break;
		}
		return MOD_RES_PASSTHRU;
	}

	void OnWebIRCAuth(LocalUser* user, const WebIRC::FlagMap* flags) CXX11_OVERRIDE
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
						ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "BUG: OnWebIRCAuth(%s): socket type %d is unknown!",
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
						ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "BUG: OnWebIRCAuth(%s): socket type %d is unknown!",
							user->uuid.c_str(), user->server_sa.family());
						return;
				}
			}
		}
	}

	void OnWhois(Whois::Context& whois) CXX11_OVERRIDE
	{
		if (!whois.IsSelfWhois() && !whois.GetSource()->HasPrivPermission("users/auspex"))
			return;

		// If these fields are not set then the client is not using a gateway.
		const std::string* realhost = cmd.realhost.get(whois.GetTarget());
		const std::string* realip = cmd.realip.get(whois.GetTarget());
		if (!realhost || !realip)
			return;

		const std::string* gateway = cmd.gateway.get(whois.GetTarget());
		if (gateway)
			whois.SendLine(RPL_WHOISGATEWAY, *realhost, *realip, "is connected via the " + *gateway + " WebIRC gateway");
		else
			whois.SendLine(RPL_WHOISGATEWAY, *realhost, *realip, "is connected via an ident gateway");
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Enables forwarding the real IP address of a user from a gateway to the IRC server", VF_VENDOR);
	}
};

MODULE_INIT(ModuleCgiIRC)
