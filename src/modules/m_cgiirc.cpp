/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2004 ChatSpike-Dev.
 *                       E-mail:
 *               <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *               <omster@gmail.com>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include <vector>
#include <string>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "users.h"
#include "modules.h"
#include "helperfuncs.h"
#include "dns.h"

/* $ModDesc: Change user's hosts connecting from known CGI:IRC hosts */


/* We need this for checking our user hasnt /quit before we finish our lookup */
extern userrec* fd_ref_table[MAX_DESCRIPTORS];

enum CGItype { PASS, IDENT, PASSFIRST, IDENTFIRST };

class CGIhost : public classbase
{
public:
	std::string hostmask;
	CGItype type;

	CGIhost(const std::string &mask = "", CGItype t = IDENTFIRST)
	: hostmask(mask), type(t)
	{
	}
};

typedef std::vector<CGIhost> CGIHostlist;

class CGIResolver : public Resolver
{
	std::string typ;
	int theirfd;
	userrec* them;
	bool notify;
 public:
	CGIResolver(bool NotifyOpers, const std::string &source, bool forward, userrec* u, int userfd, const std::string &type)
		: Resolver(source, forward), typ(type), theirfd(userfd), them(u), notify(NotifyOpers) { }

	virtual void OnLookupComplete(const std::string &result)
	{
		/* Check the user still exists */
		if ((them) && (them == fd_ref_table[theirfd]))
		{
			if (notify)
				WriteOpers("*** Connecting user %s detected as using CGI:IRC (%s), changing real host to %s from %s", them->nick, them->host, result.c_str(), typ.c_str());

			strlcpy(them->host, result.c_str(), 63);
			strlcpy(them->dhost, result.c_str(), 63);
			strlcpy(them->ident, "~cgiirc", 8);
		}
	}

	virtual void OnError(ResolverError e)
	{
		if ((them) && (them == fd_ref_table[theirfd]))
		{
			if (notify)
				WriteOpers("*** Connecting user %s detected as using CGI:IRC (%s), but their host can't be resolved from their %s!", them->nick, them->host,typ.c_str());
		}
	}

	virtual ~CGIResolver()
	{
	}
};

class ModuleCgiIRC : public Module
{
	Server *Srv;
	bool NotifyOpers;
	CGIHostlist Hosts;
public:
	ModuleCgiIRC(Server* Me) : Module::Module(Me)
	{
		Srv = Me;
		OnRehash("");
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = List[I_OnUserRegister] = List[I_OnCleanup] = List[I_OnSyncUserMetaData] = List[I_OnDecodeMetaData] = List[I_OnUserQuit] = 1;
	}
	
	virtual Priority Prioritize()
	{
		// We want to get here before m_cloaking and m_hostchange etc
		return PRIORITY_FIRST;
	}

	virtual void OnRehash(const std::string &parameter)
	{
		ConfigReader Conf;
		
		NotifyOpers = Conf.ReadFlag("cgiirc", "opernotice", 0);	// If we send an oper notice when a CGI:IRC has their host changed.
		
		if(Conf.GetError() == CONF_VALUE_NOT_FOUND)
			NotifyOpers = true;
		
		for(int i = 0; i < Conf.Enumerate("cgihost"); i++)
		{
			std::string hostmask = Conf.ReadValue("cgihost", "mask", i); // An allowed CGI:IRC host
			std::string type = Conf.ReadValue("cgihost", "type", i); // What type of user-munging we do on this host.
			
			if(hostmask.length())
			{
				Hosts.push_back(CGIhost(hostmask));
				
				if(type == "pass")
					Hosts.back().type = PASS;
				else if(type == "ident")
					Hosts.back().type = IDENT;
				else if(type == "passfirst")
					Hosts.back().type = PASSFIRST;
			}
			else
			{
				log(DEBUG, "m_cgiirc.so: Invalid <cgihost:mask> value in config: %s", hostmask.c_str());
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
	
	virtual void OnSyncUserMetaData(userrec* user, Module* proto, void* opaque, const std::string &extname)
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

	virtual void OnUserQuit(userrec* user, const std::string &message)
	{
		OnCleanup(TYPE_USER, user);
	}
	

	virtual void OnUserRegister(userrec* user)
	{
		log(DEBUG, "m_cgiirc.so: User %s registering, %s %s", user->nick,user->host,insp_ntoa(user->ip4));
		
		for(CGIHostlist::iterator iter = Hosts.begin(); iter != Hosts.end(); iter++)
		{
			log(DEBUG, "m_cgiirc.so: Matching %s against (%s or %s)", iter->hostmask.c_str(), user->host, insp_ntoa(user->ip4));
			
			if(Srv->MatchText(user->host, iter->hostmask) || Srv->MatchText(insp_ntoa(user->ip4), iter->hostmask))
			{
				// Deal with it...
				log(DEBUG, "m_cgiirc.so: Handling CGI:IRC user: %s (%s) matched %s", user->GetFullRealHost(), insp_ntoa(user->ip4), iter->hostmask.c_str());
				
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
				
				return;
			}
		}
	}

	bool CheckPass(userrec* user)
	{
		log(DEBUG, "m_cgiirc.so: CheckPass(%s) - %s", user->nick, user->password);
		
		if(IsValidHost(user->password))
		{
			user->Extend("cgiirc_realhost", new std::string(user->host));
			user->Extend("cgiirc_realip", new std::string(insp_ntoa(user->ip4)));
			strlcpy(user->host, user->password, 64);
			strlcpy(user->dhost, user->password, 64);
			
			if(insp_aton(user->password, &user->ip4))
			{
				/* We were given a IP in the password, we don't do DNS so they get this is as their host as well. */
				log(DEBUG, "m_cgiirc.so: Got an IP in the user's password");

				if(NotifyOpers)
					WriteOpers("*** Connecting user %s detected as using CGI:IRC (%s), changing real host to %s from PASS", user->nick, user->host, user->password);
			}
			else
			{
				/* We got as resolved hostname in the password. */
				log(DEBUG, "m_cgiirc.so: Got a hostname in the user's password");

				try
				{
					CGIResolver* r = new CGIResolver(NotifyOpers, user->password, false, user, user->fd, "PASS");
					Srv->AddResolver(r);
				}
				catch (ModuleException& e)
				{
					if (NotifyOpers)
						WriteOpers("*** Connecting user %s detected as using CGI:IRC (%s), but i could not resolve their hostname!", user->nick, user->host);
				}
			}
			
			*user->password = 0;

			/*if(NotifyOpers)
				WriteOpers("*** Connecting user %s detected as using CGI:IRC (%s), changing real host to %s from PASS", user->nick, user->host, user->password);*/

			return true;
		}
		else
		{
			log(DEBUG, "m_cgiirc.so: User's password was not a valid host");
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
		user->Extend("cgiirc_realip", new std::string(insp_ntoa(user->ip4)));
		insp_aton(newip, &user->ip4);

		try
		{
			log(DEBUG,"MAKE RESOLVER: %s %d %s",newip, user->fd, "IDENT");
			CGIResolver* r = new CGIResolver(NotifyOpers, newip, false, user, user->fd, "IDENT");
			Srv->AddResolver(r);
		}
		catch (ModuleException& e)
		{
			strlcpy(user->host, newip, 16);
			strlcpy(user->dhost, newip, 16);
			strlcpy(user->ident, "~cgiirc", 8);

			if(NotifyOpers)
				 WriteOpers("*** Connecting user %s detected as using CGI:IRC (%s), but i could not resolve their hostname!", user->nick, user->host);
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
		return Version(1,0,0,0,VF_VENDOR);
	}
	
};

class ModuleCgiIRCFactory : public ModuleFactory
{
 public:
	ModuleCgiIRCFactory()
	{
	}
	
	~ModuleCgiIRCFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleCgiIRC(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleCgiIRCFactory;
}
