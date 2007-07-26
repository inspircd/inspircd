/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "users.h"
#include "modules.h"
#include "dns.h"
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

	CGIhost(const std::string &mask = "", CGItype t = IDENTFIRST, const std::string &password ="")
	: hostmask(mask), type(t), password(password)
	{
	}
};
typedef std::vector<CGIhost> CGIHostlist;

class cmd_webirc : public command_t
{
	InspIRCd* Me;
	CGIHostlist Hosts;
	bool notify;
	public:
		cmd_webirc(InspIRCd* Me, CGIHostlist &Hosts, bool notify) : command_t(Me, "WEBIRC", 0, 4, true), Hosts(Hosts), notify(notify)
		{
			this->source = "m_cgiirc.so";
			this->syntax = "password client hostname ip";
		}
		CmdResult Handle(const char** parameters, int pcnt, userrec *user)
		{
			if(user->registered == REG_ALL)
				return CMD_FAILURE;
			
			for(CGIHostlist::iterator iter = Hosts.begin(); iter != Hosts.end(); iter++)
			{
				if(ServerInstance->MatchText(user->host, iter->hostmask) || ServerInstance->MatchText(user->GetIPString(), iter->hostmask))
				{
					if(iter->type == WEBIRC && parameters[0] == iter->password)
					{
						user->Extend("cgiirc_realhost", new std::string(user->host));
						user->Extend("cgiirc_realip", new std::string(user->GetIPString()));
						if (notify)
							ServerInstance->WriteOpers("*** Connecting user %s detected as using CGI:IRC (%s), changing real host to %s from %s", user->nick, user->host, parameters[2], user->host);
						user->Extend("cgiirc_webirc_hostname", new std::string(parameters[2]));
						user->Extend("cgiirc_webirc_ip", new std::string(parameters[3]));
						return CMD_LOCALONLY;
					}
				}
			}
			return CMD_FAILURE;
		}
};


/** Resolver for CGI:IRC hostnames encoded in ident/GECOS
 */
class CGIResolver : public Resolver
{
	std::string typ;
	int theirfd;
	userrec* them;
	bool notify;
 public:
	CGIResolver(Module* me, InspIRCd* ServerInstance, bool NotifyOpers, const std::string &source, bool forward, userrec* u, int userfd, const std::string &type, bool &cached)
		: Resolver(ServerInstance, source, forward ? DNS_QUERY_A : DNS_QUERY_PTR4, cached, me), typ(type), theirfd(userfd), them(u), notify(NotifyOpers) { }

	virtual void OnLookupComplete(const std::string &result, unsigned int ttl, bool cached)
	{
		/* Check the user still exists */
		if ((them) && (them == ServerInstance->SE->GetRef(theirfd)))
		{
			if (notify)
				ServerInstance->WriteOpers("*** Connecting user %s detected as using CGI:IRC (%s), changing real host to %s from %s", them->nick, them->host, result.c_str(), typ.c_str());

			strlcpy(them->host, result.c_str(), 63);
			strlcpy(them->dhost, result.c_str(), 63);
			strlcpy(them->ident, "~cgiirc", 8);
			them->InvalidateCache();
		}
	}

	virtual void OnError(ResolverError e, const std::string &errormessage)
	{
		if ((them) && (them == ServerInstance->SE->GetRef(theirfd)))
		{
			if (notify)
				ServerInstance->WriteOpers("*** Connecting user %s detected as using CGI:IRC (%s), but their host can't be resolved from their %s!", them->nick, them->host,typ.c_str());
		}
	}

	virtual ~CGIResolver()
	{
	}
};

class ModuleCgiIRC : public Module
{
	cmd_webirc* mycommand;
	bool NotifyOpers;
	CGIHostlist Hosts;
public:
	ModuleCgiIRC(InspIRCd* Me) : Module(Me)
	{
		
		OnRehash(NULL,"");
		mycommand=new cmd_webirc(Me, Hosts, NotifyOpers);
		ServerInstance->AddCommand(mycommand);
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = List[I_OnUserRegister] = List[I_OnCleanup] = List[I_OnSyncUserMetaData] = List[I_OnDecodeMetaData] = List[I_OnUserQuit] = List[I_OnUserConnect] = 1;
	}
	
	virtual Priority Prioritize()
	{
		// We want to get here before m_cloaking and m_hostchange etc
		return PRIORITY_FIRST;
	}

	virtual void OnRehash(userrec* user, const std::string &parameter)
	{
		ConfigReader Conf(ServerInstance);
		
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
						ServerInstance->Log(DEFAULT, "m_cgiirc: Missing password in config: %s", hostmask.c_str());
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
				ServerInstance->Log(DEFAULT, "m_cgiirc.so: Invalid <cgihost:mask> value in config: %s", hostmask.c_str());
				continue;
			}
		}
	}

	virtual void OnCleanup(int target_type, void* item)
	{
		if(target_type == TYPE_USER)
		{
			userrec* user = (userrec*)item;
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
	
	virtual void OnSyncUserMetaData(userrec* user, Module* proto, void* opaque, const std::string &extname, bool displayable)
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
			userrec* dest = (userrec*)target;
			std::string* bleh;
			if(((extname == "cgiirc_realhost") || (extname == "cgiirc_realip")) && (!dest->GetExt(extname, bleh)))
			{
				dest->Extend(extname, new std::string(extdata));
			}
		}
	}

	virtual void OnUserQuit(userrec* user, const std::string &message, const std::string &oper_message)
	{
		OnCleanup(TYPE_USER, user);
	}
	

	virtual int OnUserRegister(userrec* user)
	{	
		for(CGIHostlist::iterator iter = Hosts.begin(); iter != Hosts.end(); iter++)
		{			
			if(ServerInstance->MatchText(user->host, iter->hostmask) || ServerInstance->MatchText(user->GetIPString(), iter->hostmask))
			{
				// Deal with it...
				if(iter->type == PASS)
				{
					CheckPass(user); // We do nothing if it fails so...
				}
				else if(iter->type == PASSFIRST && !CheckPass(user))
				{
					// If the password lookup failed, try the ident
					CheckIdent(user);	// If this fails too, do nothing
				}
				else if(iter->type == IDENT)
				{
					CheckIdent(user); // Nothing on failure.
				}
				else if(iter->type == IDENTFIRST && !CheckIdent(user))
				{
					// If the ident lookup fails, try the password.
					CheckPass(user);
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

	virtual void OnUserConnect(userrec* user)
	{
		std::string *webirc_hostname, *webirc_ip;
		if(user->GetExt("cgiirc_webirc_hostname", webirc_hostname))
		{
			strlcpy(user->host,webirc_hostname->c_str(),63);
			strlcpy(user->dhost,webirc_hostname->c_str(),63);
			delete webirc_hostname;
			user->InvalidateCache();
			user->Shrink("cgiirc_webirc_hostname");
		}
		if(user->GetExt("cgiirc_webirc_ip", webirc_ip))
		{
			bool valid=false;
			user->RemoveCloneCounts();
#ifdef IPV6
			valid = (inet_pton(AF_INET6, webirc_ip->c_str(), &((sockaddr_in6*)user->ip)->sin6_addr) > 0); 

			if(!valid)
				valid = (inet_aton(webirc_ip->c_str(), &((sockaddr_in*)user->ip)->sin_addr));
#else
			if (inet_aton(webirc_ip->c_str(), &((sockaddr_in*)user->ip)->sin_addr))
				valid = true;
#endif

			delete webirc_ip;
			user->InvalidateCache();
			user->Shrink("cgiirc_webirc_ip");
			ServerInstance->AddLocalClone(user);
			ServerInstance->AddGlobalClone(user);
			user->CheckClass();
		}
	}

	bool CheckPass(userrec* user)
	{
		if(IsValidHost(user->password))
		{
			user->Extend("cgiirc_realhost", new std::string(user->host));
			user->Extend("cgiirc_realip", new std::string(user->GetIPString()));
			strlcpy(user->host, user->password, 64);
			strlcpy(user->dhost, user->password, 64);
			user->InvalidateCache();

			bool valid = false;
			user->RemoveCloneCounts();
#ifdef IPV6
			if (user->GetProtocolFamily() == AF_INET6)
				valid = (inet_pton(AF_INET6, user->password, &((sockaddr_in6*)user->ip)->sin6_addr) > 0);
			else
				valid = (inet_aton(user->password, &((sockaddr_in*)user->ip)->sin_addr));
#else
			if (inet_aton(user->password, &((sockaddr_in*)user->ip)->sin_addr))
				valid = true;
#endif
			ServerInstance->AddLocalClone(user);
			ServerInstance->AddGlobalClone(user);
			user->CheckClass();

			if (valid)
			{
				/* We were given a IP in the password, we don't do DNS so they get this is as their host as well. */
				if(NotifyOpers)
					ServerInstance->WriteOpers("*** Connecting user %s detected as using CGI:IRC (%s), changing real host to %s from PASS", user->nick, user->host, user->password);
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
						ServerInstance->WriteOpers("*** Connecting user %s detected as using CGI:IRC (%s), but i could not resolve their hostname!", user->nick, user->host);
				}
			}
			
			*user->password = 0;

			/*if(NotifyOpers)
				ServerInstance->WriteOpers("*** Connecting user %s detected as using CGI:IRC (%s), changing real host to %s from PASS", user->nick, user->host, user->password);*/

			return true;
		}
		
		return false;
	}
	
	bool CheckIdent(userrec* user)
	{
		int ip[4];
		char* ident;
		char newip[16];
		int len = strlen(user->ident);
		
		if(len == 8)
			ident = user->ident;
		else if(len == 9 && *user->ident == '~')
			ident = user->ident+1;
		else
			return false;
	
		for(int i = 0; i < 4; i++)
			if(!HexToInt(ip[i], ident + i*2))
				return false;

		snprintf(newip, 16, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
			
		user->Extend("cgiirc_realhost", new std::string(user->host));
		user->Extend("cgiirc_realip", new std::string(user->GetIPString()));
		user->RemoveCloneCounts();
#ifdef IPV6
		if (user->GetProtocolFamily() == AF_INET6)
			inet_pton(AF_INET6, newip, &((sockaddr_in6*)user->ip)->sin6_addr);
		else
#endif
		inet_aton(newip, &((sockaddr_in*)user->ip)->sin_addr);
		ServerInstance->AddLocalClone(user);
		ServerInstance->AddGlobalClone(user);
		user->CheckClass();
		try
		{
			strlcpy(user->host, newip, 16);
			strlcpy(user->dhost, newip, 16);
			strlcpy(user->ident, "~cgiirc", 8);

			bool cached;
			CGIResolver* r = new CGIResolver(this, ServerInstance, NotifyOpers, newip, false, user, user->GetFd(), "IDENT", cached);
			ServerInstance->AddResolver(r, cached);
		}
		catch (...)
		{
			strlcpy(user->host, newip, 16);
			strlcpy(user->dhost, newip, 16);
			strlcpy(user->ident, "~cgiirc", 8);
			user->InvalidateCache();

			if(NotifyOpers)
				 ServerInstance->WriteOpers("*** Connecting user %s detected as using CGI:IRC (%s), but i could not resolve their hostname!", user->nick, user->host);
		}
		/*strlcpy(user->host, newip, 16);
		strlcpy(user->dhost, newip, 16);
		strlcpy(user->ident, "~cgiirc", 8);*/

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
		return Version(1,1,0,0,VF_VENDOR,API_VERSION);
	}
	
};

MODULE_INIT(ModuleCgiIRC)
