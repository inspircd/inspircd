/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 John Brooks <john.brooks@dereferenced.net>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2006-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
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
#include "xline.h"
#include "modules/dns.h"

enum CGItype { PASS, IDENT, PASSFIRST, IDENTFIRST, WEBIRC };

// We need this method up here so that it can be accessed from anywhere
static void ChangeIP(User* user, const std::string& newip)
{
	ServerInstance->Users->RemoveCloneCounts(user);
	user->SetClientIP(newip.c_str());
	ServerInstance->Users->AddClone(user);
}

/** Holds a CGI site's details
 */
class CGIhost
{
public:
	std::string hostmask;
	CGItype type;
	std::string password;

	CGIhost(const std::string &mask, CGItype t, const std::string &spassword)
	: hostmask(mask), type(t), password(spassword)
	{
	}
};
typedef std::vector<CGIhost> CGIHostlist;

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
class CommandWebirc : public Command
{
 public:
	bool notify;
	StringExtItem gateway;
	StringExtItem realhost;
	StringExtItem realip;

	CGIHostlist Hosts;
	CommandWebirc(Module* Creator)
		: Command(Creator, "WEBIRC", 4)
		, gateway("cgiirc_gateway", ExtensionItem::EXT_USER, Creator)
		, realhost("cgiirc_realhost", ExtensionItem::EXT_USER, Creator)
		, realip("cgiirc_realip", ExtensionItem::EXT_USER, Creator)
		{
			allow_empty_last_param = false;
			works_before_reg = true;
			this->syntax = "password gateway hostname ip";
		}
		CmdResult Handle(const std::vector<std::string> &parameters, User *user)
		{
			if(user->registered == REG_ALL)
				return CMD_FAILURE;

			irc::sockets::sockaddrs ipaddr;
			if (!irc::sockets::aptosa(parameters[3], 0, ipaddr))
			{
				IS_LOCAL(user)->CommandFloodPenalty += 5000;
				ServerInstance->SNO->WriteGlobalSno('a', "Connecting user %s tried to use WEBIRC but gave an invalid IP address.", user->GetFullRealHost().c_str());
				return CMD_FAILURE;
			}

			for(CGIHostlist::iterator iter = Hosts.begin(); iter != Hosts.end(); iter++)
			{
				if(InspIRCd::Match(user->host, iter->hostmask, ascii_case_insensitive_map) || InspIRCd::MatchCIDR(user->GetIPString(), iter->hostmask, ascii_case_insensitive_map))
				{
					if(iter->type == WEBIRC && parameters[0] == iter->password)
					{
						gateway.set(user, parameters[1]);
						realhost.set(user, user->host);
						realip.set(user, user->GetIPString());

						// Check if we're happy with the provided hostname. If it's problematic then make sure we won't set a host later, just the IP
						bool host_ok = (parameters[2].length() <= ServerInstance->Config->Limits.MaxHost);
						const std::string& newhost = (host_ok ? parameters[2] : parameters[3]);

						if (notify)
							ServerInstance->SNO->WriteGlobalSno('w', "Connecting user %s detected as using CGI:IRC (%s), changing real host to %s from %s", user->nick.c_str(), user->host.c_str(), newhost.c_str(), user->host.c_str());

						// Where the magic happens - change their IP
						ChangeIP(user, parameters[3]);
						// And follow this up by changing their host
						user->host = user->dhost = newhost;
						user->InvalidateCache();

						return CMD_SUCCESS;
					}
				}
			}

			IS_LOCAL(user)->CommandFloodPenalty += 5000;
			ServerInstance->SNO->WriteGlobalSno('w', "Connecting user %s tried to use WEBIRC, but didn't match any configured webirc blocks.", user->GetFullRealHost().c_str());
			return CMD_FAILURE;
		}
};


/** Resolver for CGI:IRC hostnames encoded in ident/GECOS
 */
class CGIResolver : public DNS::Request
{
	std::string typ;
	std::string theiruid;
	LocalIntExt& waiting;
	bool notify;
 public:
	CGIResolver(DNS::Manager *mgr, Module* me, bool NotifyOpers, const std::string &source, LocalUser* u,
			const std::string &ttype, LocalIntExt& ext)
		: DNS::Request(mgr, me, source, DNS::QUERY_PTR), typ(ttype), theiruid(u->uuid),
		waiting(ext), notify(NotifyOpers)
	{
	}

	void OnLookupComplete(const DNS::Query *r) CXX11_OVERRIDE
	{
		/* Check the user still exists */
		User* them = ServerInstance->FindUUID(theiruid);
		if ((them) && (!them->quitting))
		{
			LocalUser* lu = IS_LOCAL(them);
			if (!lu)
				return;

			const DNS::ResourceRecord &ans_record = r->answers[0];
			if (ans_record.rdata.empty() || ans_record.rdata.length() > ServerInstance->Config->Limits.MaxHost)
				return;

			if (notify)
				ServerInstance->SNO->WriteGlobalSno('w', "Connecting user %s detected as using CGI:IRC (%s), changing real host to %s from %s", them->nick.c_str(), them->host.c_str(), ans_record.rdata.c_str(), typ.c_str());

			them->host = them->dhost = ans_record.rdata;
			them->InvalidateCache();
			lu->CheckLines(true);
		}
	}

	void OnError(const DNS::Query *r) CXX11_OVERRIDE
	{
		if (!notify)
			return;

		User* them = ServerInstance->FindUUID(theiruid);
		if ((them) && (!them->quitting))
		{
			ServerInstance->SNO->WriteToSnoMask('w', "Connecting user %s detected as using CGI:IRC (%s), but their host can't be resolved from their %s!", them->nick.c_str(), them->host.c_str(), typ.c_str());
		}
	}

	~CGIResolver()
	{
		User* them = ServerInstance->FindUUID(theiruid);
		if (!them)
			return;
		int count = waiting.get(them);
		if (count)
			waiting.set(them, count - 1);
	}
};

class ModuleCgiIRC : public Module
{
	CommandWebirc cmd;
	LocalIntExt waiting;
	dynamic_reference<DNS::Manager> DNS;

	static void RecheckClass(LocalUser* user)
	{
		user->MyClass = NULL;
		user->SetClass();
		user->CheckClass();
	}

	void HandleIdentOrPass(LocalUser* user, const std::string& newip, bool was_pass)
	{
		cmd.realhost.set(user, user->host);
		cmd.realip.set(user, user->GetIPString());
		ChangeIP(user, newip);
		user->host = user->dhost = user->GetIPString();
		user->InvalidateCache();
		RecheckClass(user);

		// Don't create the resolver if the core couldn't put the user in a connect class or when dns is disabled
		if (user->quitting || !DNS || !user->MyClass->resolvehostnames)
			return;

		CGIResolver* r = new CGIResolver(*this->DNS, this, cmd.notify, newip, user, (was_pass ? "PASS" : "IDENT"), waiting);
		try
		{
			waiting.set(user, waiting.get(user) + 1);
			this->DNS->Process(r);
		}
		catch (DNS::Exception &ex)
		{
			int count = waiting.get(user);
			if (count)
				waiting.set(user, count - 1);
			delete r;
			if (cmd.notify)
				 ServerInstance->SNO->WriteToSnoMask('w', "Connecting user %s detected as using CGI:IRC (%s), but I could not resolve their hostname; %s", user->nick.c_str(), user->host.c_str(), ex.GetReason().c_str());
		}
	}

public:
	ModuleCgiIRC()
		: cmd(this)
		, waiting("cgiirc-delay", ExtensionItem::EXT_USER, this)
		, DNS(this, "DNS")
	{
	}

	void init() CXX11_OVERRIDE
	{
		ServerInstance->SNO->EnableSnomask('w', "CGIIRC");
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		cmd.Hosts.clear();

		// Do we send an oper notice when a CGI:IRC has their host changed?
		cmd.notify = ServerInstance->Config->ConfValue("cgiirc")->getBool("opernotice", true);

		ConfigTagList tags = ServerInstance->Config->ConfTags("cgihost");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			ConfigTag* tag = i->second;
			std::string hostmask = tag->getString("mask"); // An allowed CGI:IRC host
			std::string type = tag->getString("type"); // What type of user-munging we do on this host.
			std::string password = tag->getString("password");

			if(hostmask.length())
			{
				if (type == "webirc" && password.empty())
				{
					ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Missing password in config: %s", hostmask.c_str());
				}
				else
				{
					CGItype cgitype;
					if (type == "pass")
						cgitype = PASS;
					else if (type == "ident")
						cgitype = IDENT;
					else if (type == "passfirst")
						cgitype = PASSFIRST;
					else if (type == "webirc")
						cgitype = WEBIRC;
					else
					{
						cgitype = PASS;
						ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Invalid <cgihost:type> value in config: %s, setting it to \"pass\"", type.c_str());
					}

					cmd.Hosts.push_back(CGIhost(hostmask, cgitype, password));
				}
			}
			else
			{
				ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Invalid <cgihost:mask> value in config: %s", hostmask.c_str());
				continue;
			}
		}
	}

	ModResult OnCheckReady(LocalUser *user) CXX11_OVERRIDE
	{
		if (waiting.get(user))
			return MOD_RES_DENY;

		if (!cmd.realip.get(user))
			return MOD_RES_PASSTHRU;

		RecheckClass(user);
		if (user->quitting)
			return MOD_RES_DENY;

		user->CheckLines(true);
		if (user->quitting)
			return MOD_RES_DENY;

		return MOD_RES_PASSTHRU;
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
		for(CGIHostlist::iterator iter = cmd.Hosts.begin(); iter != cmd.Hosts.end(); iter++)
		{
			if(InspIRCd::Match(user->host, iter->hostmask, ascii_case_insensitive_map) || InspIRCd::MatchCIDR(user->GetIPString(), iter->hostmask, ascii_case_insensitive_map))
			{
				// Deal with it...
				if(iter->type == PASS)
				{
					CheckPass(user); // We do nothing if it fails so...
					user->CheckLines(true);
				}
				else if(iter->type == PASSFIRST && !CheckPass(user))
				{
					// If the password lookup failed, try the ident
					CheckIdent(user);	// If this fails too, do nothing
					user->CheckLines(true);
				}
				else if(iter->type == IDENT)
				{
					CheckIdent(user); // Nothing on failure.
					user->CheckLines(true);
				}
				else if(iter->type == IDENTFIRST && !CheckIdent(user))
				{
					// If the ident lookup fails, try the password.
					CheckPass(user);
					user->CheckLines(true);
				}
				else if(iter->type == WEBIRC)
				{
					// We don't need to do anything here
				}
				return MOD_RES_PASSTHRU;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	bool CheckPass(LocalUser* user)
	{
		if(IsValidHost(user->password))
		{
			HandleIdentOrPass(user, user->password, true);
			user->password.clear();
			return true;
		}

		return false;
	}

	bool CheckIdent(LocalUser* user)
	{
		const char* ident;
		in_addr newip;

		if (user->ident.length() == 8)
			ident = user->ident.c_str();
		else if (user->ident.length() == 9 && user->ident[0] == '~')
			ident = user->ident.c_str() + 1;
		else
			return false;

		errno = 0;
		unsigned long ipaddr = strtoul(ident, NULL, 16);
		if (errno)
			return false;
		newip.s_addr = htonl(ipaddr);
		std::string newipstr(inet_ntoa(newip));

		user->ident = "~cgiirc";
		HandleIdentOrPass(user, newipstr, false);

		return true;
	}

	bool IsValidHost(const std::string &host)
	{
		if(!host.size() || host.size() > ServerInstance->Config->Limits.MaxHost)
			return false;

		for(unsigned int i = 0; i < host.size(); i++)
		{
			if(	((host[i] >= '0') && (host[i] <= '9')) ||
					((host[i] >= 'A') && (host[i] <= 'Z')) ||
					((host[i] >= 'a') && (host[i] <= 'z')) ||
					((host[i] == '-') && (i > 0) && (i+1 < host.size()) && (host[i-1] != '.') && (host[i+1] != '.')) ||
					((host[i] == '.') && (i > 0) && (i+1 < host.size())) )

				continue;
			else
				return false;
		}

		return true;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Change user's hosts connecting from known CGI:IRC hosts",VF_VENDOR);
	}
};

MODULE_INIT(ModuleCgiIRC)
