/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
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
		enum EnumBanaction { I_UNKNOWN, I_KILL, I_ZLINE, I_KLINE, I_GLINE, I_MARK };
		enum EnumType { A_RECORD, A_BITMASK };
		std::string name, ident, host, domain, reason;
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
	std::string theiruid;
	LocalStringExt& nameExt;
	LocalIntExt& countExt;
	DNSBLConfEntry *ConfEntry;

 public:

	DNSBLResolver(Module *me, LocalStringExt& match, LocalIntExt& ctr, const std::string &hostname, LocalUser* u, DNSBLConfEntry *conf, bool &cached)
		: Resolver(hostname, DNS_QUERY_A, cached, me), theiruid(u->uuid), nameExt(match), countExt(ctr), ConfEntry(conf)
	{
	}

	/* Note: This may be called multiple times for multiple A record results */
	virtual void OnLookupComplete(const std::string &result, unsigned int ttl, bool cached)
	{
		/* Check the user still exists */
		LocalUser* them = (LocalUser*)ServerInstance->FindUUID(theiruid);
		if (them)
		{
			int i = countExt.get(them);
			if (i)
				countExt.set(them, i - 1);
			// Now we calculate the bitmask: 256*(256*(256*a+b)+c)+d
			if(result.length())
			{
				unsigned int bitmask = 0, record = 0;
				bool match = false;
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
							ServerInstance->Users->QuitUser(them, std::string("Killed (") + reason + ")");
							break;
						}
						case DNSBLConfEntry::I_MARK:
						{
							if (!ConfEntry->ident.empty())
							{
								them->WriteServ("304 " + them->nick + " :Your ident has been set to " + ConfEntry->ident + " because you matched " + reason);
								them->ChangeIdent(ConfEntry->ident.c_str());
							}

							if (!ConfEntry->host.empty())
							{
								them->WriteServ("304 " + them->nick + " :Your host has been set to " + ConfEntry->host + " because you matched " + reason);
								them->ChangeDisplayedHost(ConfEntry->host.c_str());
							}

							nameExt.set(them, ConfEntry->name);
							break;
						}
						case DNSBLConfEntry::I_KLINE:
						{
							KLine* kl = new KLine(ServerInstance->Time(), ConfEntry->duration, ServerInstance->Config->ServerName.c_str(), reason.c_str(),
									"*", them->GetIPString());
							if (ServerInstance->XLines->AddLine(kl,NULL))
							{
								ServerInstance->SNO->WriteGlobalSno('x',"K:line added due to DNSBL match on *@%s to expire on %s: %s", 
									them->GetIPString(), ServerInstance->TimeString(kl->expiry).c_str(), reason.c_str());
								ServerInstance->XLines->ApplyLines();
							}
							else
								delete kl;
							break;
						}
						case DNSBLConfEntry::I_GLINE:
						{
							GLine* gl = new GLine(ServerInstance->Time(), ConfEntry->duration, ServerInstance->Config->ServerName.c_str(), reason.c_str(),
									"*", them->GetIPString());
							if (ServerInstance->XLines->AddLine(gl,NULL))
							{
								ServerInstance->SNO->WriteGlobalSno('x',"G:line added due to DNSBL match on *@%s to expire on %s: %s", 
									them->GetIPString(), ServerInstance->TimeString(gl->expiry).c_str(), reason.c_str());
								ServerInstance->XLines->ApplyLines();
							}
							else
								delete gl;
							break;
						}
						case DNSBLConfEntry::I_ZLINE:
						{
							ZLine* zl = new ZLine(ServerInstance->Time(), ConfEntry->duration, ServerInstance->Config->ServerName.c_str(), reason.c_str(),
									them->GetIPString());
							if (ServerInstance->XLines->AddLine(zl,NULL))
							{
								ServerInstance->SNO->WriteGlobalSno('x',"Z:line added due to DNSBL match on *@%s to expire on %s: %s", 
									them->GetIPString(), ServerInstance->TimeString(zl->expiry).c_str(), reason.c_str());
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

					ServerInstance->SNO->WriteGlobalSno('a', "Connecting user %s detected as being on a DNS blacklist (%s) with result %d", them->GetFullRealHost().c_str(), ConfEntry->domain.c_str(), (ConfEntry->type==DNSBLConfEntry::A_BITMASK) ? bitmask : record);
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
		LocalUser* them = (LocalUser*)ServerInstance->FindUUID(theiruid);
		if (them)
		{
			int i = countExt.get(them);
			if (i)
				countExt.set(them, i - 1);
		}
	}

	virtual ~DNSBLResolver()
	{
	}
};

class ModuleDNSBL : public Module
{
	std::vector<DNSBLConfEntry *> DNSBLConfEntries;
	LocalStringExt nameExt;
	LocalIntExt countExt;

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
		if(action.compare("MARK")==0)
			return DNSBLConfEntry::I_MARK;

		return DNSBLConfEntry::I_UNKNOWN;
	}
 public:
	ModuleDNSBL() : nameExt("dnsbl_match", this), countExt("dnsbl_pending", this) { }

	void init()
	{
		ReadConf();
		ServerInstance->Modules->AddService(nameExt);
		ServerInstance->Modules->AddService(countExt);
		Implementation eventlist[] = { I_OnRehash, I_OnUserInit, I_OnStats, I_OnSetConnectClass, I_OnCheckReady };
		ServerInstance->Modules->Attach(eventlist, this, 5);
	}

	virtual ~ModuleDNSBL()
	{
		ClearEntries();
	}

	Version GetVersion()
	{
		return Version("Provides handling of DNS blacklists", VF_VENDOR);
	}

	/** Clear entries and free the mem it was using
	 */
	void ClearEntries()
	{
		for (std::vector<DNSBLConfEntry *>::iterator i = DNSBLConfEntries.begin(); i != DNSBLConfEntries.end(); i++)
			delete *i;
		DNSBLConfEntries.clear();
	}

	/** Fill our conf vector with data
	 */
	void ReadConf()
	{
		ClearEntries();

		ConfigTagList dnsbls = ServerInstance->Config->ConfTags("dnsbl");
		for(ConfigIter i = dnsbls.first; i != dnsbls.second; ++i)
		{
			ConfigTag* tag = i->second;
			DNSBLConfEntry *e = new DNSBLConfEntry();

			e->name = tag->getString("name");
			e->ident = tag->getString("ident");
			e->host = tag->getString("host");
			e->reason = tag->getString("reason");
			e->domain = tag->getString("domain");

			if (tag->getString("type") == "bitmask")
			{
				e->type = DNSBLConfEntry::A_BITMASK;
				e->bitmask = tag->getInt("bitmask");
			}
			else
			{
				memset(e->records, 0, sizeof(e->records));
				e->type = DNSBLConfEntry::A_RECORD;
				irc::portparser portrange(tag->getString("records"), false);
				long item = -1;
				while ((item = portrange.GetToken()))
					e->records[item] = 1;
			}

			e->banaction = str2banaction(tag->getString("action"));
			e->duration = ServerInstance->Duration(tag->getString("duration", "60"));

			/* Use portparser for record replies */

			/* yeah, logic here is a little messy */
			if ((e->bitmask <= 0) && (DNSBLConfEntry::A_BITMASK == e->type))
			{
				ServerInstance->SNO->WriteGlobalSno('a', "DNSBL(%s): invalid bitmask",tag->getTagLocation().c_str());
			}
			else if (e->name.empty())
			{
				ServerInstance->SNO->WriteGlobalSno('a', "DNSBL(%s): Invalid name",tag->getTagLocation().c_str());
			}
			else if (e->domain.empty())
			{
				ServerInstance->SNO->WriteGlobalSno('a', "DNSBL(%s): Invalid domain",tag->getTagLocation().c_str());
			}
			else if (e->banaction == DNSBLConfEntry::I_UNKNOWN)
			{
				ServerInstance->SNO->WriteGlobalSno('a', "DNSBL(%s): Invalid banaction",tag->getTagLocation().c_str());
			}
			else if (e->duration <= 0)
			{
				ServerInstance->SNO->WriteGlobalSno('a', "DNSBL(%s): Invalid duration",tag->getTagLocation().c_str());
			}
			else
			{
				if (e->reason.empty())
				{
					ServerInstance->SNO->WriteGlobalSno('a', "DNSBL(%s): empty reason, using defaults",tag->getTagLocation().c_str());
					e->reason = "Your IP has been blacklisted.";
				}

				/* add it, all is ok */
				DNSBLConfEntries.push_back(e);
				continue;
			}

			/* delete and drop it, error somewhere */
			delete e;
		}
	}

	void OnRehash(User* user)
	{
		ReadConf();
	}

	void OnUserInit(LocalUser* user)
	{
		if (user->exempt)
			return;

		/* following code taken from bopm, reverses an IP address. */
		struct in_addr in;
		unsigned char a, b, c, d;
		char reversedipbuf[128];
		std::string reversedip;
		bool success;

		success = inet_aton(user->GetIPString(), &in);

		if (!success)
			return;

		d = (unsigned char) (in.s_addr >> 24) & 0xFF;
		c = (unsigned char) (in.s_addr >> 16) & 0xFF;
		b = (unsigned char) (in.s_addr >> 8) & 0xFF;
		a = (unsigned char) in.s_addr & 0xFF;

		snprintf(reversedipbuf, 128, "%d.%d.%d.%d", d, c, b, a);
		reversedip = std::string(reversedipbuf);

		// For each DNSBL, we will run through this lookup
		unsigned int i = 0;
		while (i < DNSBLConfEntries.size())
		{
			// Fill hostname with a dnsbl style host (d.c.b.a.domain.tld)
			std::string hostname = reversedip + "." + DNSBLConfEntries[i]->domain;

			/* now we'd need to fire off lookups for `hostname'. */
			bool cached;
			DNSBLResolver *r = new DNSBLResolver(this, nameExt, countExt, hostname, user, DNSBLConfEntries[i], cached);
			ServerInstance->AddResolver(r, cached);
			i++;
		}
		countExt.set(user, i);
	}

	ModResult OnSetConnectClass(LocalUser* user, ConnectClass* myclass)
	{
		std::string dnsbl;
		if (!myclass->config->readString("dnsbl", dnsbl))
			return MOD_RES_PASSTHRU;
		std::string* match = nameExt.get(user);
		std::string myname = match ? *match : "";
		if (dnsbl == myname)
			return MOD_RES_PASSTHRU;
		return MOD_RES_DENY;
	}
	
	ModResult OnCheckReady(LocalUser *user)
	{
		if (countExt.get(user))
			return MOD_RES_DENY;
		return MOD_RES_PASSTHRU;
	}

	ModResult OnStats(char symbol, User* user, string_list &results)
	{
		if (symbol != 'd')
			return MOD_RES_PASSTHRU;

		unsigned long total_hits = 0, total_misses = 0;

		for (std::vector<DNSBLConfEntry*>::iterator i = DNSBLConfEntries.begin(); i != DNSBLConfEntries.end(); i++)
		{
			total_hits += (*i)->stats_hits;
			total_misses += (*i)->stats_misses;

			results.push_back(std::string(ServerInstance->Config->ServerName.c_str()) + " 304 " + user->nick + " :DNSBLSTATS DNSbl \"" + (*i)->name + "\" had " +
					ConvToStr((*i)->stats_hits) + " hits and " + ConvToStr((*i)->stats_misses) + " misses");
		}

		results.push_back(std::string(ServerInstance->Config->ServerName.c_str()) + " 304 " + user->nick + " :DNSBLSTATS Total hits: " + ConvToStr(total_hits));
		results.push_back(std::string(ServerInstance->Config->ServerName.c_str()) + " 304 " + user->nick + " :DNSBLSTATS Total misses: " + ConvToStr(total_misses));

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleDNSBL)
