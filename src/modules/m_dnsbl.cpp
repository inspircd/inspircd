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
#include "xline.h"

#ifndef WINDOWS
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

/* $ModDesc: Provides handling of DNS blacklists */

/* Class holding data for a single entry */
class DNSBLConfEntry
{
	public:
		enum EnumBanaction { I_UNKNOWN, I_KILL, I_ZLINE, I_KLINE, I_GLINE };
		enum EnumType { A_RECORD, A_BITMASK };
		std::string name, domain, reason;
		EnumBanaction banaction;
		EnumType type;
		long duration;
		int bitmask;
		unsigned char records[256];
		unsigned long stats_hits, stats_misses;
		DNSBLConfEntry(): type(A_BITMASK),duration(86400),bitmask(0),stats_hits(0), stats_misses(0) {}
		~DNSBLConfEntry() { }
};


/** Resolver for CGI:IRC hostnames encoded in ident/GECOS
 */
class DNSBLResolver : public Resolver
{
	int theirfd;
	User* them;
	DNSBLConfEntry *ConfEntry;

 public:

	DNSBLResolver(Module *me, InspIRCd *ServerInstance, const std::string &hostname, User* u, int userfd, DNSBLConfEntry *conf, bool &cached)
		: Resolver(ServerInstance, hostname, DNS_QUERY_A, cached, me)
	{
		theirfd = userfd;
		them = u;
		ConfEntry = conf;
	}

	/* Note: This may be called multiple times for multiple A record results */
	virtual void OnLookupComplete(const std::string &result, unsigned int ttl, bool cached, int resultnum = 0)
	{
		/* for bitmask reply types, we arent interested in any but the first result (number 0) */
		if ((ConfEntry->type == DNSBLConfEntry::A_BITMASK) && (resultnum))
			return;

		/* Check the user still exists */
		if ((them) && (them == ServerInstance->SE->GetRef(theirfd)))
		{
			// Now we calculate the bitmask: 256*(256*(256*a+b)+c)+d
			if(result.length())
			{
				unsigned int bitmask = 0, record = 0;
				bool show = false, match = false;
				in_addr resultip;

				inet_aton(result.c_str(), &resultip);

				switch (ConfEntry->type)
				{
					case DNSBLConfEntry::A_BITMASK:
						bitmask = resultip.s_addr >> 24; /* Last octet (network byte order) */
						bitmask &= ConfEntry->bitmask;
						match = (bitmask != 0);
					break;
					case DNSBLConfEntry::A_RECORD:
						record = resultip.s_addr >> 24; /* Last octet */
						match = (ConfEntry->records[record] == 1);
					break;
				}

				if (match)
				{
					std::string reason = ConfEntry->reason;
					std::string::size_type x = reason.find("%ip%");
					while (x != std::string::npos)
					{
						reason.erase(x, 4);
						reason.insert(x, them->GetIPString());
						x = reason.find("%ip%");
					}

					ConfEntry->stats_hits++;

					switch (ConfEntry->banaction)
					{
						case DNSBLConfEntry::I_KILL:
						{
							User::QuitUser(ServerInstance, them, std::string("Killed (") + reason + ")");
							break;
						}
						case DNSBLConfEntry::I_KLINE:
						{
							KLine* kl = new KLine(ServerInstance, ServerInstance->Time(), ConfEntry->duration, ServerInstance->Config->ServerName, reason.c_str(),
									"*", them->GetIPString());
							if (ServerInstance->XLines->AddLine(kl,NULL))
							{
								ServerInstance->XLines->ApplyLines();
							}
							else
								delete kl;
							break;
						}
						case DNSBLConfEntry::I_GLINE:
						{
							GLine* gl = new GLine(ServerInstance, ServerInstance->Time(), ConfEntry->duration, ServerInstance->Config->ServerName, reason.c_str(),
									"*", them->GetIPString());
							if (ServerInstance->XLines->AddLine(gl,NULL))
							{
								ServerInstance->XLines->ApplyLines();
							}
							else
								delete gl;
							break;
						}
						case DNSBLConfEntry::I_ZLINE:
						{
							ZLine* zl = new ZLine(ServerInstance, ServerInstance->Time(), ConfEntry->duration, ServerInstance->Config->ServerName, reason.c_str(),
									them->GetIPString());
							if (ServerInstance->XLines->AddLine(zl,NULL))
							{
								ServerInstance->XLines->ApplyLines();
							}
							else 
								delete zl;
							break;
						}
						case DNSBLConfEntry::I_UNKNOWN:
						{
							break;
						}
						break;
					}

					if (show)
					{
						ServerInstance->WriteOpers("*** Connecting user %s detected as being on a DNS blacklist (%s) with result %d", them->GetFullRealHost(), ConfEntry->name.c_str(), bitmask);
					}
				}
				else
					ConfEntry->stats_misses++;
			}
			else
				ConfEntry->stats_misses++;
		}
	}

	virtual void OnError(ResolverError e, const std::string &errormessage)
	{
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
	ModuleDNSBL(InspIRCd *Me) : Module(Me)
	{
		ReadConf();
		Implementation eventlist[] = { I_OnRehash, I_OnUserRegister, I_OnStats };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	virtual ~ModuleDNSBL()
	{
		ClearEntries();
	}

	virtual Version GetVersion()
	{
		return Version(2, 0, 0, 1, VF_VENDOR, API_VERSION);
	}


	/** Clear entries and free the mem it was using
	 */
	void ClearEntries()
	{
		std::vector<DNSBLConfEntry *>::iterator i;
		for (std::vector<DNSBLConfEntry *>::iterator i = DNSBLConfEntries.begin(); i != DNSBLConfEntries.end(); i++)
			delete *i;
		DNSBLConfEntries.clear();
	}

	/** Fill our conf vector with data
	 */
	virtual void ReadConf()
	{
		ConfigReader *MyConf = new ConfigReader(ServerInstance);
		ClearEntries();

		for (int i=0; i< MyConf->Enumerate("dnsbl"); i++)
		{
			DNSBLConfEntry *e = new DNSBLConfEntry();

			e->name = MyConf->ReadValue("dnsbl", "name", i);
			e->reason = MyConf->ReadValue("dnsbl", "reason", i);
			e->domain = MyConf->ReadValue("dnsbl", "domain", i);

			if (MyConf->ReadValue("dnsbl", "type", i) == "bitmask")
			{
				e->type = DNSBLConfEntry::A_BITMASK;
				e->bitmask = MyConf->ReadInteger("dnsbl", "bitmask", i, false);
			}
			else
			{
				memset(e->records, 0, 256);
				e->type = DNSBLConfEntry::A_RECORD;
				irc::portparser portrange(MyConf->ReadValue("dnsbl", "records", i), false);
				long item = -1;
				while ((item = portrange.GetToken()))
					e->records[item] = 1;
			}

			e->banaction = str2banaction(MyConf->ReadValue("dnsbl", "action", i));
			e->duration = ServerInstance->Duration(MyConf->ReadValue("dnsbl", "duration", i));
			
			/* Use portparser for record replies */

			/* yeah, logic here is a little messy */
			if (e->bitmask <= 0)
			{
				ServerInstance->WriteOpers("*** DNSBL(#%d): invalid bitmask",i);
			}
			else if (e->name.empty())
			{
				ServerInstance->WriteOpers("*** DNSBL(#%d): Invalid name",i);
			}
			else if (e->domain.empty())
			{
				ServerInstance->WriteOpers("*** DNSBL(#%d): Invalid domain",i);
			}
			else if (e->banaction == DNSBLConfEntry::I_UNKNOWN)
			{
				ServerInstance->WriteOpers("*** DNSBL(#%d): Invalid banaction", i);
			}
			else
			{
				if (e->reason.empty())
				{
					ServerInstance->WriteOpers("*** DNSBL(#%d): empty reason, using defaults",i);
					e->reason = "Your IP has been blacklisted.";
				}

				/* add it, all is ok */
				DNSBLConfEntries.push_back(e);
				continue;
			}

			/* delete and drop it, error somewhere */
			delete e;
		}

		delete MyConf;
	}

	virtual void OnRehash(User* user, const std::string &parameter)
	{
		ReadConf();
	}

	virtual int OnUserRegister(User* user)
	{
		/* only do lookups on local users */
		if (IS_LOCAL(user))
		{
			/* following code taken from bopm, reverses an IP address. */
			struct in_addr in;
			unsigned char a, b, c, d;
			char reversedipbuf[128];
			std::string reversedip;
			bool success = false;

			if (!inet_aton(user->GetIPString(), &in))
			{
#ifdef IPV6
				/* We could have an ipv6 address here */
				std::string x = user->GetIPString();
				/* Is it a 4in6 address? (Compensate for this kernel kludge that people love) */
				if (x.find("0::ffff:") == 0)
				{
					x.erase(x.begin(), x.begin() + 8);
					if (inet_aton(x.c_str(), &in))
						success = true;
				}
#endif
			}
			else
			{
				success = true;
			}

			if (!success)
				return 0;

			d = (unsigned char) (in.s_addr >> 24) & 0xFF;
			c = (unsigned char) (in.s_addr >> 16) & 0xFF;
			b = (unsigned char) (in.s_addr >> 8) & 0xFF;
			a = (unsigned char) in.s_addr & 0xFF;

			snprintf(reversedipbuf, 128, "%d.%d.%d.%d", d, c, b, a);
			reversedip = std::string(reversedipbuf);

			// For each DNSBL, we will run through this lookup
			for (std::vector<DNSBLConfEntry *>::iterator i = DNSBLConfEntries.begin(); i != DNSBLConfEntries.end(); i++)
			{
				// Fill hostname with a dnsbl style host (d.c.b.a.domain.tld)
				std::string hostname = reversedip + "." + (*i)->domain;

				/* now we'd need to fire off lookups for `hostname'. */
				bool cached;
				DNSBLResolver *r = new DNSBLResolver(this, ServerInstance, hostname, user, user->GetFd(), *i, cached);
				ServerInstance->AddResolver(r, cached);
			}
		}

		/* don't do anything with this hot potato */
		return 0;
	}
	
	virtual int OnStats(char symbol, User* user, string_list &results)
	{
		if (symbol != 'd')
			return 0;
		
		unsigned long total_hits = 0, total_misses = 0;

		for (std::vector<DNSBLConfEntry*>::iterator i = DNSBLConfEntries.begin(); i != DNSBLConfEntries.end(); i++)
		{
			total_hits += (*i)->stats_hits;
			total_misses += (*i)->stats_misses;
			
			results.push_back(std::string(ServerInstance->Config->ServerName) + " 304 " + user->nick + " :DNSBLSTATS DNSbl \"" + (*i)->name + "\" had " +
					ConvToStr((*i)->stats_hits) + " hits and " + ConvToStr((*i)->stats_misses) + " misses");
		}
		
		results.push_back(std::string(ServerInstance->Config->ServerName) + " 304 " + user->nick + " :DNSBLSTATS Total hits: " + ConvToStr(total_hits));
		results.push_back(std::string(ServerInstance->Config->ServerName) + " 304 " + user->nick + " :DNSBLSTATS Total misses: " + ConvToStr(total_misses));
		
		return 0;
	}
};

MODULE_INIT(ModuleDNSBL)
