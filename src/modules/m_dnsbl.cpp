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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>
#include "inspircd.h"
#include "dns.h"
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides handling of DNS blacklists */

/* Class holding data for a single entry */
class DNSBLConfEntry
{
	public:
		enum EnumBanaction { I_UNKNOWN, I_KILL, I_ZLINE, I_KLINE, I_GLINE };
		std::string name, domain, reason;
		EnumBanaction banaction;
		long duration;
		int bitmask;
		DNSBLConfEntry(): duration(86400),bitmask(0) {}
};


/** Resolver for CGI:IRC hostnames encoded in ident/GECOS
 */
class DNSBLResolver : public Resolver
{
	int theirfd;
	userrec* them;
	DNSBLConfEntry *ConfEntry;

    public:
	DNSBLResolver(Module *me, InspIRCd *ServerInstance, const std::string &hostname, userrec* u, int userfd, DNSBLConfEntry *conf)
		: Resolver(ServerInstance, hostname, DNS_QUERY_A, me)
	{
		theirfd = userfd;
		them = u;
		ConfEntry = conf;
	}

	virtual void OnLookupComplete(const std::string &result)
	{
		/* Check the user still exists */
		if ((them) && (them == ServerInstance->SE->GetRef(theirfd)))
		{
			ServerInstance->Log(DEBUG, "m_dnsbl:  %s got a result from dnsbl %s", them->nick, ConfEntry->name.c_str());

			// Now we calculate the bitmask: 256*(256*(256*a+b)+c)+d
			if(result.length())
			{
				unsigned int bitmask=0;
				unsigned int octetpos=0;
				std::string tmp = result;

				while(tmp.length()>0)
				{
					std::string octet;
					unsigned int lastdot = tmp.rfind(".");

					if (lastdot == std::string::npos)
					{
						octet=tmp;
						tmp.clear();
					}
					else
					{
						octet=tmp.substr(lastdot+1,tmp.length()-lastdot+1);
						tmp.resize(lastdot);
					}

					bitmask += (256 ^ octetpos) * atoi(octet.c_str());
					octetpos += 1;
				}

				bitmask &= ConfEntry->bitmask;

				if (bitmask != 0)
				{
					std::string reason = ConfEntry->reason;

					while (int pos = reason.find("%ip%") != std::string::npos)
					{
						reason.replace(pos, 4, them->GetIPString());
					}

					ServerInstance->WriteOpers("*** Connecting user %s detected as being on a DNS blacklist (%s) with result %d", them->GetFullRealHost(), ConfEntry->name.c_str(), bitmask);

					switch (ConfEntry->banaction)
					{
						case DNSBLConfEntry::I_KILL:
						{
							them->QuitUser(ServerInstance, them, std::string("Killed (") + reason + ")");
							break;
						}
						case DNSBLConfEntry::I_KLINE:
						{
							ServerInstance->AddKLine(ConfEntry->duration, ServerInstance->Config->ServerName, reason, std::string("*@") + them->GetIPString());
							break;
						}
						case DNSBLConfEntry::I_GLINE:
						{
							ServerInstance->AddGLine(ConfEntry->duration, ServerInstance->Config->ServerName, reason, std::string("*@") + them->GetIPString());
							break;
						}
						case DNSBLConfEntry::I_ZLINE:
						{
							ServerInstance->AddZLine(ConfEntry->duration, ServerInstance->Config->ServerName, reason, them->GetIPString());
							break;
						}
						case DNSBLConfEntry::I_UNKNOWN:
						{
							break;
						}
						break;
					}
				}
			}
		}
	}

	virtual void OnError(ResolverError e, const std::string &errormessage)
	{
		/*
		this just means they don't appear in the respective dnsbl
		if ((them) && (them == ServerInstance->SE->GetRef(theirfd)))
		{
		}
		*/
		/* Check the user still exists */
		if ((them) && (them == ServerInstance->SE->GetRef(theirfd)))
		{
			ServerInstance->Log(DEBUG, "m_dnsbl:  %s got an error while resolving for dnsbl %s", them->nick, ConfEntry->name.c_str());
		}
	}
 
	virtual ~DNSBLResolver()
	{
	}
};

class ModuleDNSBL : public Module
{
 private:
	std::vector<DNSBLConfEntry *> DNSBLConfEntries;
	
	/*
	 *	Convert a string to EnumBanaction
	 */
	DNSBLConfEntry::EnumBanaction str2banaction(const std::string &action)
	{
		if(action.compare("KILL")==0)
			return DNSBLConfEntry::I_KILL;
		if(action.compare("KLINE")==0)
			return DNSBLConfEntry::I_KLINE;
		if(action.compare("ZLINE")==0)
			return DNSBLConfEntry::I_ZLINE;
		if(action.compare("GLINE")==0)
			return DNSBLConfEntry::I_GLINE;
	
		return DNSBLConfEntry::I_UNKNOWN;
	}
 public:
	ModuleDNSBL(InspIRCd *Me) : Module::Module(Me)
	{
		ReadConf();
	}
	
	virtual ~ModuleDNSBL()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(2, 0, 0, 0, 0, API_VERSION);
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = List[I_OnUserRegister] = 1;
	}

	/*
	 * Fill our conf vector with data
	 */	
	virtual void ReadConf()
	{
		ConfigReader *MyConf = new ConfigReader(ServerInstance);
		DNSBLConfEntries.clear();

		for (int i=0; i< MyConf->Enumerate("dnsbl"); i++)
		{
			DNSBLConfEntry *e = new DNSBLConfEntry();

			e->name = MyConf->ReadValue("dnsbl", "name", i);
			e->reason = MyConf->ReadValue("dnsbl", "reason", i);
			e->domain = MyConf->ReadValue("dnsbl", "domain", i);
			e->banaction = str2banaction(MyConf->ReadValue("dnsbl", "action", i));
			e->duration = ServerInstance->Duration(MyConf->ReadValue("dnsbl", "duration", i).c_str());
			e->bitmask = MyConf->ReadInteger("dnsbl", "bitmask", i, false);

			/* yeah, logic here is a little messy */
			if (e->bitmask <= 0)
			{
				ServerInstance->WriteOpers("*** DNSBL(#%d): invalid bitmask",i);
			}
			else if (e->name == "")
			{
				ServerInstance->WriteOpers("*** DNSBL(#%d): Invalid name",i);
			}
			else if (e->domain == "")
			{
				ServerInstance->WriteOpers("*** DNSBL(#%d): Invalid domain",i);
			}
			else if (e->banaction == DNSBLConfEntry::I_UNKNOWN)
			{
				ServerInstance->WriteOpers("*** DNSBL(#%d): Invalid banaction", i);
			}
			else
			{
				if (e->reason == "")
				{
					ServerInstance->WriteOpers("*** DNSBL(#%d): empty reason, using defaults",i);
					e->reason = "Your IP has been blacklisted.";
				}

				/* add it, all is ok */
				DNSBLConfEntries.push_back(e);
				delete MyConf;
				continue;
			}

			/* delete and drop it, error somewhere */
			delete e;
		}

		delete MyConf;
	}
	
	
	virtual void OnRehash(const std::string &parameter)
	{
		ReadConf();
	}

	/*
	 * We will check each user that connects *locally* (userrec::fd>0)
	 */
	virtual int OnUserRegister(userrec* user)
	{
		if (IS_LOCAL(user))
		{
			/* following code taken from bopm, reverses an IP address. */
			struct in_addr in;
			unsigned char a, b, c, d;
			char reversedipbuf[128];
			std::string reversedip;

			if (!inet_aton(user->GetIPString(), &in))
			{
				ServerInstance->WriteOpers("Invalid IP address in m_dnsbl! Bailing check");
				return 0;
			}

			d = (unsigned char) (in.s_addr >> 24) & 0xFF;
			c = (unsigned char) (in.s_addr >> 16) & 0xFF;
			b = (unsigned char) (in.s_addr >> 8) & 0xFF;
			a = (unsigned char) in.s_addr & 0xFF;

			snprintf(reversedipbuf, 128, "%d.%d.%d.%d", d, c, b, a);
			reversedip = std::string(reversedipbuf);

/*
	this is satmd's old code
			std::string reversedip;
			std::string userip = user->GetIPString();
			std::string tempip = userip;

			// reversedip will created in there
			while (tempip.length()>0)
			{
				unsigned int lastdot=tempip.rfind(".");
				if (lastdot == std::string::npos)
				{
					reversedip+=tempip;
					tempip.clear();
				}
				else
				{
					reversedip += tempip.substr(lastdot+1,tempip.length()-lastdot+1);
					reversedip += ".";
					tempip.resize(lastdot);
				}
			}
*/
		
			// For each DNSBL, we will run through this lookup
			for (std::vector<DNSBLConfEntry *>::iterator i = DNSBLConfEntries.begin(); i != DNSBLConfEntries.end(); i++)
			{
				// Fill hostname with a dnsbl style host (d.c.b.a.domain.tld)
				std::string hostname=reversedip+"."+ (*i)->domain;

				ServerInstance->Log(DEBUG, "m_dnsbl: sending %s for resolution", hostname.c_str());

				/* now we'd need to fire off lookups for `hostname'. */
				DNSBLResolver *r = new DNSBLResolver(this, ServerInstance, hostname, user, user->GetFd(), *i);
				ServerInstance->AddResolver(r);
			}
		}

		/* don't do anything with this hot potato */
		return 0;
	}
};

// stuff down here is the module-factory stuff.

class ModuleDNSBLFactory : public ModuleFactory
{
 public:
	ModuleDNSBLFactory()
	{
	}
	
	~ModuleDNSBLFactory()
	{
	}
	
	virtual Module *CreateModule(InspIRCd *Me)
	{
		return new ModuleDNSBL(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleDNSBLFactory;
}

