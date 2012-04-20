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

#ifndef WINDOWS
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

/* $ModDesc: Change user's hosts connecting from known CGI:IRC hosts */

enum CGItype { INVALID, PASS, IDENT, PASSFIRST, IDENTFIRST, WEBIRC };


/** Holds a CGI site's details
 */
class CGIhost
{
public:
	std::string hostmask;
	CGItype type;
	std::string password;

	CGIhost(const std::string &mask = "", CGItype t = IDENTFIRST, const std::string &spassword ="")
	: hostmask(mask), type(t), password(spassword)
	{
	}
};
typedef std::vector<CGIhost> CGIHostlist;

/*
 * WEBIRC
 *  This is used for the webirc method of CGIIRC auth, and is (really) the best way to do these things.
 *  Syntax: WEBIRC password client hostname ip
 *  Where password is a shared key, client is the name of the "client" and version (e.g. cgiirc), hostname
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
	StringExtItem realhost;
	StringExtItem realip;
	LocalStringExt webirc_hostname;
	LocalStringExt webirc_ip;

	CGIHostlist Hosts;
	CommandWebirc(Module* Creator)
		: Command(Creator, "WEBIRC", 4),
		  realhost("cgiirc_realhost", Creator), realip("cgiirc_realip", Creator),
		  webirc_hostname("cgiirc_webirc_hostname", Creator), webirc_ip("cgiirc_webirc_ip", Creator)
		{
			works_before_reg = true;
			this->syntax = "password client hostname ip";
		}
		CmdResult Handle(const std::vector<std::string> &parameters, User *user)
		{
			if(user->registered == REG_ALL)
				return CMD_FAILURE;

			for(CGIHostlist::iterator iter = Hosts.begin(); iter != Hosts.end(); iter++)
			{
				if(InspIRCd::Match(user->host, iter->hostmask, ascii_case_insensitive_map) || InspIRCd::MatchCIDR(user->GetIPString(), iter->hostmask, ascii_case_insensitive_map))
				{
					if(iter->type == WEBIRC && parameters[0] == iter->password)
					{
						realhost.set(user, user->host);
						realip.set(user, user->GetIPString());
						if (notify)
							ServerInstance->SNO->WriteGlobalSno('a', "Connecting user %s detected as using CGI:IRC (%s), changing real host to %s from %s", user->nick.c_str(), user->host.c_str(), parameters[2].c_str(), user->host.c_str());
						webirc_hostname.set(user, parameters[2]);
						webirc_ip.set(user, parameters[3]);
						return CMD_SUCCESS;
					}
				}
			}

			ServerInstance->SNO->WriteGlobalSno('a', "Connecting user %s tried to use WEBIRC, but didn't match any configured webirc blocks.", user->GetFullRealHost().c_str());
			return CMD_FAILURE;
		}
};


/** Resolver for CGI:IRC hostnames encoded in ident/GECOS
 */
class CGIResolver : public Resolver
{
	std::string typ;
	std::string theiruid;
	LocalIntExt& waiting;
	bool notify;
 public:
	CGIResolver(Module* me, bool NotifyOpers, const std::string &source, bool forward, LocalUser* u,
			const std::string &type, bool &cached, LocalIntExt& ext)
		: Resolver(source, forward ? DNS_QUERY_A : DNS_QUERY_PTR4, cached, me), typ(type), theiruid(u->uuid),
		waiting(ext), notify(NotifyOpers)
	{
	}

	virtual void OnLookupComplete(const std::string &result, unsigned int ttl, bool cached)
	{
		/* Check the user still exists */
		User* them = ServerInstance->FindUUID(theiruid);
		if (them)
		{
			if (notify)
				ServerInstance->SNO->WriteGlobalSno('a', "Connecting user %s detected as using CGI:IRC (%s), changing real host to %s from %s", them->nick.c_str(), them->host.c_str(), result.c_str(), typ.c_str());

			if (result.length() > 64)
				return;
			them->host = result;
			them->dhost = result;
			them->InvalidateCache();
			them->CheckLines(true);
		}
	}

	virtual void OnError(ResolverError e, const std::string &errormessage)
	{
		User* them = ServerInstance->FindUUID(theiruid);
		if (them)
		{
			if (notify)
				ServerInstance->SNO->WriteToSnoMask('a', "Connecting user %s detected as using CGI:IRC (%s), but their host can't be resolved from their %s!", them->nick.c_str(), them->host.c_str(), typ.c_str());
		}
	}

	virtual ~CGIResolver()
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
public:
	ModuleCgiIRC() : cmd(this), waiting("cgiirc-delay", this)
	{
	}

	void init()
	{
		OnRehash(NULL);
		ServerInstance->AddCommand(&cmd);
		ServerInstance->Extensions.Register(&cmd.realhost);
		ServerInstance->Extensions.Register(&cmd.realip);
		ServerInstance->Extensions.Register(&cmd.webirc_hostname);
		ServerInstance->Extensions.Register(&cmd.webirc_ip);

		Implementation eventlist[] = { I_OnRehash, I_OnUserRegister, I_OnCheckReady, I_OnUserConnect };
		ServerInstance->Modules->Attach(eventlist, this, 4);
	}

	void Prioritize()
	{
		ServerInstance->Modules->SetPriority(this, I_OnUserConnect, PRIORITY_FIRST);
		Module* umodes = ServerInstance->Modules->Find("m_conn_umodes.so");
		ServerInstance->Modules->SetPriority(this, I_OnUserConnect, PRIORITY_BEFORE, &umodes);
	}

	void OnRehash(User* user)
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
				if (type == "webirc" && !password.length()) {
						ServerInstance->Logs->Log("CONFIG",DEFAULT, "m_cgiirc: Missing password in config: %s", hostmask.c_str());
				}
				else
				{
					CGItype cgitype = INVALID;
					if (type == "pass")
						cgitype = PASS;
					else if (type == "ident")
						cgitype = IDENT;
					else if (type == "passfirst")
						cgitype = PASSFIRST;
					else if (type == "webirc")
					{
						cgitype = WEBIRC;
					}

					if (cgitype == INVALID)
						cgitype = PASS;

					cmd.Hosts.push_back(CGIhost(hostmask,cgitype, password.length() ? password : "" ));
				}
			}
			else
			{
				ServerInstance->Logs->Log("CONFIG",DEFAULT, "m_cgiirc.so: Invalid <cgihost:mask> value in config: %s", hostmask.c_str());
				continue;
			}
		}
	}

	ModResult OnCheckReady(LocalUser *user)
	{
		if (waiting.get(user))
			return MOD_RES_DENY;
		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserRegister(LocalUser* user)
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

	virtual void OnUserConnect(LocalUser* user)
	{
		std::string *webirc_hostname = cmd.webirc_hostname.get(user);
		std::string *webirc_ip = cmd.webirc_ip.get(user);
		if (!webirc_ip)
			return;
		ServerInstance->Users->RemoveCloneCounts(user);
		user->SetClientIP(webirc_ip->c_str());
		user->InvalidateCache();
		if (webirc_hostname && webirc_hostname->length() < 64)
			user->host = user->dhost = *webirc_hostname;
		else
			user->host = user->dhost = user->GetIPString();
		user->InvalidateCache();
		ServerInstance->Users->AddLocalClone(user);
		ServerInstance->Users->AddGlobalClone(user);
		user->SetClass();
		user->CheckClass();
		user->CheckLines(true);
		cmd.webirc_ip.unset(user);
		cmd.webirc_hostname.unset(user);
	}

	bool CheckPass(LocalUser* user)
	{
		if(IsValidHost(user->password))
		{
			cmd.realhost.set(user, user->host);
			cmd.realip.set(user, user->GetIPString());
			user->host = user->password;
			user->dhost = user->password;
			user->InvalidateCache();

			ServerInstance->Users->RemoveCloneCounts(user);
			user->SetClientIP(user->password.c_str());
			ServerInstance->Users->AddLocalClone(user);
			ServerInstance->Users->AddGlobalClone(user);
			user->SetClass();
			user->CheckClass();

			try
			{
				bool cached;
				CGIResolver* r = new CGIResolver(this, cmd.notify, user->password, false, user, "PASS", cached, waiting);
				ServerInstance->AddResolver(r, cached);
				waiting.set(user, waiting.get(user) + 1);
			}
			catch (...)
			{
				if (cmd.notify)
					ServerInstance->SNO->WriteToSnoMask('a', "Connecting user %s detected as using CGI:IRC (%s), but I could not resolve their hostname!", user->nick.c_str(), user->host.c_str());
			}

			user->password.clear();
			return true;
		}

		return false;
	}

	bool CheckIdent(LocalUser* user)
	{
		const char* ident;
		int len = user->ident.length();
		in_addr newip;

		if(len == 8)
			ident = user->ident.c_str();
		else if(len == 9 && user->ident[0] == '~')
			ident = user->ident.c_str() + 1;
		else
			return false;

		errno = 0;
		unsigned long ipaddr = strtoul(ident, NULL, 16);
		if (errno)
			return false;
		newip.s_addr = htonl(ipaddr);
		char* newipstr = inet_ntoa(newip);

		cmd.realhost.set(user, user->host);
		cmd.realip.set(user, user->GetIPString());
		ServerInstance->Users->RemoveCloneCounts(user);
		user->SetClientIP(newipstr);
		ServerInstance->Users->AddLocalClone(user);
		ServerInstance->Users->AddGlobalClone(user);
		user->SetClass();
		user->CheckClass();
		user->host = newipstr;
		user->dhost = newipstr;
		user->ident.assign("~cgiirc", 0, 8);
		try
		{

			bool cached;
			CGIResolver* r = new CGIResolver(this, cmd.notify, newipstr, false, user, "IDENT", cached, waiting);
			ServerInstance->AddResolver(r, cached);
			waiting.set(user, waiting.get(user) + 1);
		}
		catch (...)
		{
			user->InvalidateCache();

			if(cmd.notify)
				 ServerInstance->SNO->WriteToSnoMask('a', "Connecting user %s detected as using CGI:IRC (%s), but I could not resolve their hostname!", user->nick.c_str(), user->host.c_str());
		}

		return true;
	}

	bool IsValidHost(const std::string &host)
	{
		if(!host.size() || host.size() > 64)
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

	bool IsValidIP(const std::string &ip)
	{
		if(ip.size() < 7 || ip.size() > 15)
			return false;

		short sincedot = 0;
		short dots = 0;

		for(unsigned int i = 0; i < ip.size(); i++)
		{
			if((dots <= 3) && (sincedot <= 3))
			{
				if((ip[i] >= '0') && (ip[i] <= '9'))
				{
					sincedot++;
				}
				else if(ip[i] == '.')
				{
					sincedot = 0;
					dots++;
				}
			}
			else
			{
				return false;

			}
		}

		if(dots != 3)
			return false;

		return true;
	}

	virtual ~ModuleCgiIRC()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Change user's hosts connecting from known CGI:IRC hosts",VF_VENDOR);
	}

};

MODULE_INIT(ModuleCgiIRC)
