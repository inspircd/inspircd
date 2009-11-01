/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
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
class CGIhost : public classbase
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
	CGIHostlist Hosts;
	bool notify;
	public:
		CommandWebirc(InspIRCd* Instance, bool bnotify) : Command(Instance, "WEBIRC", 0, 4, true), notify(bnotify)
		{
			this->source = "m_cgiirc.so";
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
						user->Extend("cgiirc_realhost", new std::string(user->host));
						user->Extend("cgiirc_realip", new std::string(user->GetIPString()));
						if (notify)
							ServerInstance->SNO->WriteGlobalSno('a', "Connecting user %s detected as using CGI:IRC (%s), changing real host to %s from %s", user->nick.c_str(), user->host.c_str(), parameters[2].c_str(), user->host.c_str());
						user->Extend("cgiirc_webirc_hostname", new std::string(parameters[2]));
						user->Extend("cgiirc_webirc_ip", new std::string(parameters[3]));
						return CMD_LOCALONLY;
					}
				}
			}

			ServerInstance->SNO->WriteGlobalSno('a', "Connecting user %s tried to use WEBIRC, but didn't match any configured webirc blocks.", user->GetFullRealHost().c_str());
			return CMD_FAILURE;
		}

		void SetHosts(CGIHostlist &phosts)
		{
			this->Hosts = phosts;
		}
};


/** Resolver for CGI:IRC hostnames encoded in ident/GECOS
 */
class CGIResolver : public Resolver
{
	std::string typ;
	int theirfd;
	User* them;
	bool notify;
 public:
	CGIResolver(Module* me, InspIRCd* Instance, bool NotifyOpers, const std::string &source, bool forward, User* u, int userfd, const std::string &type, bool &cached)
		: Resolver(Instance, source, forward ? DNS_QUERY_A : DNS_QUERY_PTR4, cached, me), typ(type), theirfd(userfd), them(u), notify(NotifyOpers) { }

	virtual void OnLookupComplete(const std::string &result, unsigned int ttl, bool cached)
	{
		/* Check the user still exists */
		if ((them) && (them == ServerInstance->SE->GetRef(theirfd)))
		{
			if (notify)
				ServerInstance->SNO->WriteGlobalSno('a', "Connecting user %s detected as using CGI:IRC (%s), changing real host to %s from %s", them->nick.c_str(), them->host.c_str(), result.c_str(), typ.c_str());

			them->host.assign(result,0, 64);
			them->dhost.assign(result, 0, 64);
			if (querytype)
				them->SetSockAddr(them->GetProtocolFamily(), result.c_str(), them->GetPort());
			them->ident.assign("~cgiirc", 0, 8);
			them->InvalidateCache();
			them->CheckLines(true);
		}
	}

	virtual void OnError(ResolverError e, const std::string &errormessage)
	{
		if ((them) && (them == ServerInstance->SE->GetRef(theirfd)))
		{
			if (notify)
				ServerInstance->SNO->WriteToSnoMask('a', "Connecting user %s detected as using CGI:IRC (%s), but their host can't be resolved from their %s!", them->nick.c_str(), them->host.c_str(), typ.c_str());
		}
	}

	virtual ~CGIResolver()
	{
	}
};

class ModuleCgiIRC : public Module
{
	CommandWebirc* mycommand;
	bool NotifyOpers;
	CGIHostlist Hosts;
public:
	ModuleCgiIRC(InspIRCd* Me) : Module(Me)
	{
		mycommand = new CommandWebirc(Me, NotifyOpers);
		OnRehash(NULL);
		ServerInstance->AddCommand(mycommand);

		Implementation eventlist[] = { I_OnRehash, I_OnUserRegister, I_OnCleanup, I_OnSyncUserMetaData, I_OnDecodeMetaData, I_OnUserDisconnect, I_OnUserConnect };
		ServerInstance->Modules->Attach(eventlist, this, 7);
	}


	virtual void Prioritize()
	{
		ServerInstance->Modules->SetPriority(this, I_OnUserConnect, PRIORITY_FIRST);
	}

	virtual void OnRehash(User* user)
	{
		ConfigReader Conf(ServerInstance);
		Hosts.clear();

		NotifyOpers = Conf.ReadFlag("cgiirc", "opernotice", 0);	// If we send an oper notice when a CGI:IRC has their host changed.

		if(Conf.GetError() == CONF_VALUE_NOT_FOUND)
			NotifyOpers = true;

		for(int i = 0; i < Conf.Enumerate("cgihost"); i++)
		{
			std::string hostmask = Conf.ReadValue("cgihost", "mask", i); // An allowed CGI:IRC host
			std::string type = Conf.ReadValue("cgihost", "type", i); // What type of user-munging we do on this host.
			std::string password = Conf.ReadValue("cgihost", "password", i);

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

					Hosts.push_back(CGIhost(hostmask,cgitype, password.length() ? password : "" ));
				}
			}
			else
			{
				ServerInstance->Logs->Log("CONFIG",DEFAULT, "m_cgiirc.so: Invalid <cgihost:mask> value in config: %s", hostmask.c_str());
				continue;
			}
		}

		mycommand->SetHosts(Hosts);
	}

	virtual void OnCleanup(int target_type, void* item)
	{
		if(target_type == TYPE_USER)
		{
			User* user = (User*)item;
			std::string* realhost;
			std::string* realip;

			if(user->GetExt("cgiirc_realhost", realhost))
			{
				delete realhost;
				user->Shrink("cgiirc_realhost");
			}

			if(user->GetExt("cgiirc_realip", realip))
			{
				delete realip;
				user->Shrink("cgiirc_realip");
			}
		}
	}

	virtual void OnSyncUserMetaData(User* user, Module* proto, void* opaque, const std::string &extname, bool displayable)
	{
		if((extname == "cgiirc_realhost") || (extname == "cgiirc_realip"))
		{
			std::string* data;

			if(user->GetExt(extname, data))
			{
				proto->ProtoSendMetaData(opaque, TYPE_USER, user, extname, *data);
			}
		}
	}

	virtual void OnDecodeMetaData(int target_type, void* target, const std::string &extname, const std::string &extdata)
	{
		if(target_type == TYPE_USER)
		{
			User* dest = (User*)target;
			std::string* bleh;
			if(((extname == "cgiirc_realhost") || (extname == "cgiirc_realip")) && (!dest->GetExt(extname, bleh)))
			{
				dest->Extend(extname, new std::string(extdata));
			}
		}
	}

	virtual void OnUserDisconnect(User* user)
	{
		OnCleanup(TYPE_USER, user);
	}


	virtual int OnUserRegister(User* user)
	{
		for(CGIHostlist::iterator iter = Hosts.begin(); iter != Hosts.end(); iter++)
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
				return 0;
			}
		}
		return 0;
	}

	virtual void OnUserConnect(User* user)
	{
		std::string *webirc_hostname, *webirc_ip;
		if(user->GetExt("cgiirc_webirc_hostname", webirc_hostname))
		{
			user->host.assign(*webirc_hostname, 0, 64);
			user->dhost.assign(*webirc_hostname, 0, 64);
			delete webirc_hostname;
			user->InvalidateCache();
			user->Shrink("cgiirc_webirc_hostname");
		}
		if(user->GetExt("cgiirc_webirc_ip", webirc_ip))
		{
			ServerInstance->Users->RemoveCloneCounts(user);
			user->SetSockAddr(user->GetProtocolFamily(), webirc_ip->c_str(), user->GetPort());
			delete webirc_ip;
			user->InvalidateCache();
			user->Shrink("cgiirc_webirc_ip");
			ServerInstance->Users->AddLocalClone(user);
			ServerInstance->Users->AddGlobalClone(user);

			user->SetClass();
			user->CheckClass();
			user->CheckLines(true);
		}
	}

	bool CheckPass(User* user)
	{
		if(IsValidHost(user->password))
		{
			user->Extend("cgiirc_realhost", new std::string(user->host));
			user->Extend("cgiirc_realip", new std::string(user->GetIPString()));
			user->host.assign(user->password, 0, 64);
			user->dhost.assign(user->password, 0, 64);
			user->InvalidateCache();

			bool valid = false;
			ServerInstance->Users->RemoveCloneCounts(user);
#ifdef IPV6
			if (user->GetProtocolFamily() == AF_INET6)
				valid = (inet_pton(AF_INET6, user->password.c_str(), &((sockaddr_in6*)user->ip)->sin6_addr) > 0);
			else
				valid = (inet_aton(user->password.c_str(), &((sockaddr_in*)user->ip)->sin_addr));
#else
			if (inet_aton(user->password.c_str(), &((sockaddr_in*)user->ip)->sin_addr))
				valid = true;
#endif
			ServerInstance->Users->AddLocalClone(user);
			ServerInstance->Users->AddGlobalClone(user);
			user->CheckClass();

			if (valid)
			{
				/* We were given a IP in the password, we don't do DNS so they get this is as their host as well. */
				if(NotifyOpers)
					ServerInstance->SNO->WriteGlobalSno('a', "Connecting user %s detected as using CGI:IRC (%s), changing real host to %s from PASS", user->nick.c_str(), user->host.c_str(), user->password.c_str());
			}
			else
			{
				/* We got as resolved hostname in the password. */
				try
				{

					bool cached;
					CGIResolver* r = new CGIResolver(this, ServerInstance, NotifyOpers, user->password, false, user, user->GetFd(), "PASS", cached);
					ServerInstance->AddResolver(r, cached);
				}
				catch (...)
				{
					if (NotifyOpers)
						ServerInstance->SNO->WriteToSnoMask('a', "Connecting user %s detected as using CGI:IRC (%s), but I could not resolve their hostname!", user->nick.c_str(), user->host.c_str());
				}
			}

			user->password.clear();
			return true;
		}

		return false;
	}

	bool CheckIdent(User* user)
	{
		int ip[4];
		const char* ident;
		char newip[16];
		int len = user->ident.length();

		if(len == 8)
			ident = user->ident.c_str();
		else if(len == 9 && user->ident[0] == '~')
			ident = user->ident.c_str() + 1;
		else
			return false;

		for(int i = 0; i < 4; i++)
			if(!HexToInt(ip[i], ident + i*2))
				return false;

		snprintf(newip, 16, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

		user->Extend("cgiirc_realhost", new std::string(user->host));
		user->Extend("cgiirc_realip", new std::string(user->GetIPString()));
		ServerInstance->Users->RemoveCloneCounts(user);
		user->SetSockAddr(user->GetProtocolFamily(), newip, user->GetPort());
		ServerInstance->Users->AddLocalClone(user);
		ServerInstance->Users->AddGlobalClone(user);
		user->CheckClass();
		try
		{
			user->host.assign(newip, 0, 16);
			user->dhost.assign(newip, 0, 16);
			user->ident.assign("~cgiirc", 0, 8);

			bool cached;
			CGIResolver* r = new CGIResolver(this, ServerInstance, NotifyOpers, newip, false, user, user->GetFd(), "IDENT", cached);
			ServerInstance->AddResolver(r, cached);
		}
		catch (...)
		{
			user->host.assign(newip, 0, 16);
			user->dhost.assign(newip, 0, 16);
			user->ident.assign("~cgiirc", 0, 8);
			user->InvalidateCache();

			if(NotifyOpers)
				 ServerInstance->SNO->WriteToSnoMask('a', "Connecting user %s detected as using CGI:IRC (%s), but I could not resolve their hostname!", user->nick.c_str(), user->host.c_str());
		}

		return true;
	}

	bool IsValidHost(const std::string &host)
	{
		if(!host.size())
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

	bool HexToInt(int &out, const char* in)
	{
		char ip[3];
		ip[0] = in[0];
		ip[1] = in[1];
		ip[2] = 0;
		out = strtol(ip, NULL, 16);

		if(out > 255 || out < 0)
			return false;

		return true;
	}

	virtual ~ModuleCgiIRC()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$",VF_VENDOR,API_VERSION);
	}

};

MODULE_INIT(ModuleCgiIRC)
