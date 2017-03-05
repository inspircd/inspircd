/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2006-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2006-2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 John Brooks <john.brooks@dereferenced.net>
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

/* $ModDesc: Provides handling of DNS blacklists */

/* Class holding data for a single entry */
class DNSBLConfEntry : public refcountbase
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
	reference<DNSBLConfEntry> ConfEntry;

 public:

	DNSBLResolver(Module *me, LocalStringExt& match, LocalIntExt& ctr, const std::string &hostname, LocalUser* u, reference<DNSBLConfEntry> conf, bool &cached)
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
			// All replies should be in 127.0.0.0/8
			if (result.compare(0, 4, "127.") == 0)
			{
				unsigned int bitmask = 0, record = 0;
				bool match = false;
				in_addr resultip;

				inet_aton(result.c_str(), &resultip);

				switch (ConfEntry->type)
				{
					case DNSBLConfEntry::A_BITMASK:
						// Now we calculate the bitmask: 256*(256*(256*a+b)+c)+d
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
							ServerInstance->Users->QuitUser(them, "Killed (" + reason + ")");
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
								std::string timestr = ServerInstance->TimeString(kl->expiry);
								ServerInstance->SNO->WriteGlobalSno('x',"K:line added due to DNSBL match on *@%s to expire on %s: %s",
									them->GetIPString(), timestr.c_str(), reason.c_str());
								ServerInstance->XLines->ApplyLines();
							}
							else
							{
								delete kl;
								return;
							}
							break;
						}
						case DNSBLConfEntry::I_GLINE:
						{
							GLine* gl = new GLine(ServerInstance->Time(), ConfEntry->duration, ServerInstance->Config->ServerName.c_str(), reason.c_str(),
									"*", them->GetIPString());
							if (ServerInstance->XLines->AddLine(gl,NULL))
							{
								std::string timestr = ServerInstance->TimeString(gl->expiry);
								ServerInstance->SNO->WriteGlobalSno('x',"G:line added due to DNSBL match on *@%s to expire on %s: %s",
									them->GetIPString(), timestr.c_str(), reason.c_str());
								ServerInstance->XLines->ApplyLines();
							}
							else
							{
								delete gl;
								return;
							}
							break;
						}
						case DNSBLConfEntry::I_ZLINE:
						{
							ZLine* zl = new ZLine(ServerInstance->Time(), ConfEntry->duration, ServerInstance->Config->ServerName.c_str(), reason.c_str(),
									them->GetIPString());
							if (ServerInstance->XLines->AddLine(zl,NULL))
							{
								std::string timestr = ServerInstance->TimeString(zl->expiry);
								ServerInstance->SNO->WriteGlobalSno('x',"Z:line added due to DNSBL match on *@%s to expire on %s: %s",
									them->GetIPString(), timestr.c_str(), reason.c_str());
								ServerInstance->XLines->ApplyLines();
							}
							else
							{
								delete zl;
								return;
							}
							break;
						}
						case DNSBLConfEntry::I_UNKNOWN:
						{
							break;
						}
						break;
					}

					ServerInstance->SNO->WriteGlobalSno('a', "Connecting user %s%s detected as being on a DNS blacklist (%s) with result %d", them->nick.empty() ? "<unknown>" : "", them->GetFullRealHost().c_str(), ConfEntry->domain.c_str(), (ConfEntry->type==DNSBLConfEntry::A_BITMASK) ? bitmask : record);
				}
				else
					ConfEntry->stats_misses++;
			}
			else
			{
				if (!result.empty())
					ServerInstance->SNO->WriteGlobalSno('a', "DNSBL: %s returned address outside of acceptable subnet 127.0.0.0/8: %s", ConfEntry->domain.c_str(), result.c_str());
				ConfEntry->stats_misses++;
			}
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
	std::vector<reference<DNSBLConfEntry> > DNSBLConfEntries;
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
		Implementation eventlist[] = { I_OnRehash, I_OnSetUserIP, I_OnStats, I_OnSetConnectClass, I_OnCheckReady };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	Version GetVersion()
	{
		return Version("Provides handling of DNS blacklists", VF_VENDOR);
	}

	/** Fill our conf vector with data
	 */
	void ReadConf()
	{
		DNSBLConfEntries.clear();

		ConfigTagList dnsbls = ServerInstance->Config->ConfTags("dnsbl");
		for(ConfigIter i = dnsbls.first; i != dnsbls.second; ++i)
		{
			ConfigTag* tag = i->second;
			reference<DNSBLConfEntry> e = new DNSBLConfEntry();

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
				std::string location = tag->getTagLocation();
				ServerInstance->SNO->WriteGlobalSno('a', "DNSBL(%s): invalid bitmask", location.c_str());
			}
			else if (e->name.empty())
			{
				std::string location = tag->getTagLocation();
				ServerInstance->SNO->WriteGlobalSno('a', "DNSBL(%s): Invalid name", location.c_str());
			}
			else if (e->domain.empty())
			{
				std::string location = tag->getTagLocation();
				ServerInstance->SNO->WriteGlobalSno('a', "DNSBL(%s): Invalid domain", location.c_str());
			}
			else if (e->banaction == DNSBLConfEntry::I_UNKNOWN)
			{
				std::string location = tag->getTagLocation();
				ServerInstance->SNO->WriteGlobalSno('a', "DNSBL(%s): Invalid banaction", location.c_str());
			}
			else if (e->duration <= 0)
			{
				std::string location = tag->getTagLocation();
				ServerInstance->SNO->WriteGlobalSno('a', "DNSBL(%s): Invalid duration", location.c_str());
			}
			else
			{
				if (e->reason.empty())
				{
					std::string location = tag->getTagLocation();
					ServerInstance->SNO->WriteGlobalSno('a', "DNSBL(%s): empty reason, using defaults", location.c_str());
					e->reason = "Your IP has been blacklisted.";
				}

				/* add it, all is ok */
				DNSBLConfEntries.push_back(e);
			}
		}
	}

	void OnRehash(User* user)
	{
		ReadConf();
	}

	void OnSetUserIP(LocalUser* user)
	{
		if ((user->exempt) || (user->client_sa.sa.sa_family != AF_INET))
			return;

		if (user->MyClass)
		{
			if (!user->MyClass->config->getBool("usednsbl", true))
				return;
		}
		else
			ServerInstance->Logs->Log("m_dnsbl", DEBUG, "User has no connect class in OnSetUserIP");

		unsigned char a, b, c, d;
		char reversedipbuf[128];
		std::string reversedip;

		d = (unsigned char) (user->client_sa.in4.sin_addr.s_addr >> 24) & 0xFF;
		c = (unsigned char) (user->client_sa.in4.sin_addr.s_addr >> 16) & 0xFF;
		b = (unsigned char) (user->client_sa.in4.sin_addr.s_addr >> 8) & 0xFF;
		a = (unsigned char) user->client_sa.in4.sin_addr.s_addr & 0xFF;

		snprintf(reversedipbuf, 128, "%d.%d.%d.%d", d, c, b, a);
		reversedip = std::string(reversedipbuf);

		countExt.set(user, DNSBLConfEntries.size());

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
			if (user->quitting)
				break;
			i++;
		}
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

		for (std::vector<reference<DNSBLConfEntry> >::const_iterator i = DNSBLConfEntries.begin(); i != DNSBLConfEntries.end(); ++i)
		{
			total_hits += (*i)->stats_hits;
			total_misses += (*i)->stats_misses;

			results.push_back(ServerInstance->Config->ServerName + " 304 " + user->nick + " :DNSBLSTATS DNSbl \"" + (*i)->name + "\" had " +
					ConvToStr((*i)->stats_hits) + " hits and " + ConvToStr((*i)->stats_misses) + " misses");
		}

		results.push_back(ServerInstance->Config->ServerName + " 304 " + user->nick + " :DNSBLSTATS Total hits: " + ConvToStr(total_hits));
		results.push_back(ServerInstance->Config->ServerName + " 304 " + user->nick + " :DNSBLSTATS Total misses: " + ConvToStr(total_misses));

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleDNSBL)
