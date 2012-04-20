/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2006-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2003-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 John Brooks <john.brooks@dereferenced.net>
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
#include "m_hash.h"

/* $ModDesc: Provides masking of user hostnames */
/* $ModDep: m_hash.h */

/** Handles user mode +x
 */
class CloakUser : public ModeHandler
{
 public:
	std::string prefix;
	unsigned int key1;
	unsigned int key2;
	unsigned int key3;
	unsigned int key4;
	bool ipalways;
	Module* Sender;
	Module* HashProvider;
	const char *xtab[4];

	std::string debounce_uid;
	time_t debounce_ts;
	int debounce_count;

	/** This function takes a domain name string and returns just the last two domain parts,
	 * or the last domain part if only two are available. Failing that it just returns what it was given.
	 *
	 * For example, if it is passed "svn.inspircd.org" it will return ".inspircd.org".
	 * If it is passed "brainbox.winbot.co.uk" it will return ".co.uk",
	 * and if it is passed "localhost.localdomain" it will return ".localdomain".
	 *
	 * This is used to ensure a significant part of the host is always cloaked (see Bug #216)
	 */
	std::string LastTwoDomainParts(const std::string &host)
	{
		int dots = 0;
		std::string::size_type splitdot = host.length();

		for (std::string::size_type x = host.length() - 1; x; --x)
		{
			if (host[x] == '.')
			{
				splitdot = x;
				dots++;
			}
			if (dots >= 3)
				break;
		}

		if (splitdot == host.length())
			return "";
		else
			return host.substr(splitdot);
	}

	CloakUser(InspIRCd* Instance, Module* source, Module* Hash) : ModeHandler(Instance, 'x', 0, 0, false, MODETYPE_USER, false), Sender(source), HashProvider(Hash)
	{
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool)
	{
		/* For remote clients, we dont take any action, we just allow it.
		 * The local server where they are will set their cloak instead.
		 * This is fine, as we will recieve it later.
		 */
		if (!IS_LOCAL(dest))
		{
			dest->SetMode('x',adding);
			return MODEACTION_ALLOW;
		}

		if (dest->uuid == debounce_uid && debounce_ts == ServerInstance->Time())
		{
			// prevent spamming using /mode user +x-x+x-x+x-x
			if (++debounce_count > 2)
				return MODEACTION_DENY;
		}
		else
		{
			debounce_uid = dest->uuid;
			debounce_count = 1;
			debounce_ts = ServerInstance->Time();
		}

		/* don't allow this user to spam modechanges */
		if (source == dest)
			dest->IncreasePenalty(5);

		if (adding)
		{
			if(!dest->IsModeSet('x'))
			{
				/* The mode is being turned on - so attempt to
				 * allocate the user a cloaked host using a non-reversible
				 * algorithm (its simple, but its non-reversible so the
				 * simplicity doesnt really matter). This algorithm
				 * will not work if the user has only one level of domain
				 * naming in their hostname (e.g. if they are on a lan or
				 * are connecting via localhost) -- this doesnt matter much.
				 */

				std::string* cloak;

				if (!dest->GetExt("cloaked_host", cloak))
				{
					/* Force creation of missing cloak */
					Sender->OnUserConnect(dest);
				}
				if (dest->GetExt("cloaked_host", cloak))
				{
					dest->ChangeDisplayedHost(cloak->c_str());
					dest->SetMode('x',true);
					return MODEACTION_ALLOW;
				}
			}
		}
		else
		{
			if (dest->IsModeSet('x'))
			{
				/* User is removing the mode, so just restore their real host
				 * and make it match the displayed one.
				 */
				dest->ChangeDisplayedHost(dest->host.c_str());
				dest->SetMode('x',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}

	std::string Cloak4(const char* ip)
	{
		unsigned int iv[] = { key1, key2, key3, key4 };
		irc::sepstream seps(ip, '.');
		std::string ra[4];;
		std::string octet[4];
		int i[4];

		for (int j = 0; j < 4; j++)
		{
			seps.GetToken(octet[j]);
			i[j] = atoi(octet[j].c_str());
		}

		octet[3] = octet[0] + "." + octet[1] + "." + octet[2] + "." + octet[3];
		octet[2] = octet[0] + "." + octet[1] + "." + octet[2];
		octet[1] = octet[0] + "." + octet[1];

		/* Reset the Hash module and send it our IV */
		HashResetRequest(Sender, HashProvider).Send();
		HashKeyRequest(Sender, HashProvider, iv).Send();

		/* Send the Hash module a different hex table for each octet group's Hash sum */
		for (int k = 0; k < 4; k++)
		{
			HashHexRequest(Sender, HashProvider, xtab[(iv[k]+i[k]) % 4]).Send();
			ra[k] = std::string(HashSumRequest(Sender, HashProvider, octet[k]).Send()).substr(0,6);
		}
		/* Stick them all together */
		return std::string().append(ra[0]).append(".").append(ra[1]).append(".").append(ra[2]).append(".").append(ra[3]);
	}

	std::string Cloak6(const char* ip)
	{
		unsigned int iv[] = { key1, key2, key3, key4 };
		std::vector<std::string> hashies;
		std::string item;
		int rounds = 0;

		/* Reset the Hash module and send it our IV */
		HashResetRequest(Sender, HashProvider).Send();
		HashKeyRequest(Sender, HashProvider, iv).Send();

		for (const char* input = ip; *input; input++)
		{
			item += *input;
			if (item.length() > 7)
			{
				/* Send the Hash module a different hex table for each octet group's Hash sum */
				HashHexRequest(Sender, HashProvider, xtab[(key1+rounds) % 4]).Send();
				hashies.push_back(std::string(HashSumRequest(Sender, HashProvider, item).Send()).substr(0,8));
				item.clear();
			}
			rounds++;
		}
		if (!item.empty())
		{
			/* Send the Hash module a different hex table for each octet group's Hash sum */
			HashHexRequest(Sender, HashProvider, xtab[(key1+rounds) % 4]).Send();
			hashies.push_back(std::string(HashSumRequest(Sender, HashProvider, item).Send()).substr(0,8));
			item.clear();
		}
		/* Stick them all together */
		return irc::stringjoiner(":", hashies, 0, hashies.size() - 1).GetJoined();
	}

	void DoRehash()
	{
		ConfigReader Conf(ServerInstance);
		bool lowercase;

		/* These are *not* using the need_positive parameter of ReadInteger -
		 * that will limit the valid values to only the positive values in a
		 * signed int. Instead, accept any value that fits into an int and
		 * cast it to an unsigned int. That will, a bit oddly, give us the full
		 * spectrum of an unsigned integer. - Special */
		key1 = key2 = key3 = key4 = 0;
		key1 = (unsigned int) Conf.ReadInteger("cloak","key1",0,false);
		key2 = (unsigned int) Conf.ReadInteger("cloak","key2",0,false);
		key3 = (unsigned int) Conf.ReadInteger("cloak","key3",0,false);
		key4 = (unsigned int) Conf.ReadInteger("cloak","key4",0,false);
		prefix = Conf.ReadValue("cloak","prefix",0);
		ipalways = Conf.ReadFlag("cloak", "ipalways", 0);
		lowercase = Conf.ReadFlag("cloak", "lowercase", 0);

		if (!lowercase)
		{
			xtab[0] = "F92E45D871BCA630";
			xtab[1] = "A1B9D80C72E653F4";
			xtab[2] = "1ABC078934DEF562";
			xtab[3] = "ABCDEF5678901234";
		}
		else
		{
			xtab[0] = "f92e45d871bca630";
			xtab[1] = "a1b9d80c72e653f4";
			xtab[2] = "1abc078934def562";
			xtab[3] = "abcdef5678901234";
		}

		if (prefix.empty())
			prefix = ServerInstance->Config->Network;

		if (!key1 || !key2 || !key3 || !key4)
		{
			std::string detail;
			if (!key1)
				detail = "<cloak:key1> is not valid, it may be set to a too high/low value, or it may not exist.";
			else if (!key2)
				detail = "<cloak:key2> is not valid, it may be set to a too high/low value, or it may not exist.";
			else if (!key3)
				detail = "<cloak:key3> is not valid, it may be set to a too high/low value, or it may not exist.";
			else if (!key4)
				detail = "<cloak:key4> is not valid, it may be set to a too high/low value, or it may not exist.";

			throw ModuleException("You have not defined cloak keys for m_cloaking!!! THIS IS INSECURE AND SHOULD BE CHECKED! - " + detail);
		}
	}
};


class ModuleCloaking : public Module
{
 private:

 	CloakUser* cu;
	Module* HashModule;

 public:
	ModuleCloaking(InspIRCd* Me)
		: Module(Me)
	{
		/* Attempt to locate the md5 service provider, bail if we can't find it */
		HashModule = ServerInstance->Modules->Find("m_md5.so");
		if (!HashModule)
			throw ModuleException("Can't find m_md5.so. Please load m_md5.so before m_cloaking.so.");

		cu = new CloakUser(ServerInstance, this, HashModule);

		try
		{
			OnRehash(NULL);
		}
		catch (ModuleException &e)
		{
			delete cu;
			throw e;
		}

		/* Register it with the core */
		if (!ServerInstance->Modes->AddMode(cu))
		{
			delete cu;
			throw ModuleException("Could not add new modes!");
		}

		ServerInstance->Modules->UseInterface("HashRequest");

		Implementation eventlist[] = { I_OnRehash, I_OnUserDisconnect, I_OnCleanup, I_OnCheckBan, I_OnUserConnect, I_OnSyncUserMetaData, I_OnCleanup };
		ServerInstance->Modules->Attach(eventlist, this, 6);

		CloakExistingUsers();
	}

	void OnSyncUserMetaData(User* user, Module* proto,void* opaque, const std::string &extname, bool displayable)
	{
		if ((displayable) && (extname == "cloaked_host"))
		{
			std::string* cloak;
			if (user->GetExt("cloaked_host", cloak))
				proto->ProtoSendMetaData(opaque, TYPE_USER, user, extname, *cloak);
		}
	}

	void CloakExistingUsers()
	{
		std::string* cloak;
		for (std::vector<User*>::iterator u = ServerInstance->Users->local_users.begin(); u != ServerInstance->Users->local_users.end(); u++)
		{
			if (!(*u)->GetExt("cloaked_host", cloak))
			{
				OnUserConnect(*u);
			}
		}
	}

	virtual int OnCheckBan(User* user, Channel* chan)
	{
		char mask[MAXBUF];
		std::string* tofree;
		/* Check if they have a cloaked host, but are not using it */
		if (user->GetExt("cloaked_host", tofree) && *tofree != user->dhost)
		{
			snprintf(mask, MAXBUF, "%s!%s@%s", user->nick.c_str(), user->ident.c_str(), tofree->c_str());
			for (BanList::iterator i = chan->bans.begin(); i != chan->bans.end(); i++)
			{
				if (InspIRCd::Match(mask,i->data))
					return -1;
			}
		}
		return 0;
	}

 	void Prioritize()
	{
		/* Needs to be after m_banexception etc. */
		ServerInstance->Modules->SetPriority(this, I_OnCheckBan, PRIORITY_LAST);

		/* but before m_conn_umodes, so host is generated ready to apply */
		Module *um = ServerInstance->Modules->Find("m_conn_umodes.so");
		ServerInstance->Modules->SetPriority(this, I_OnUserConnect, PRIORITY_AFTER, &um);
	}

	virtual void OnUserDisconnect(User* user)
	{
		std::string* tofree;
		if (user->GetExt("cloaked_host", tofree))
		{
			delete tofree;
			user->Shrink("cloaked_host");
		}
	}

	virtual void OnCleanup(int target_type, void* item)
	{
		if (target_type == TYPE_USER)
			OnUserDisconnect((User*)item);
	}

	virtual ~ModuleCloaking()
	{
		ServerInstance->Modes->DelMode(cu);
		delete cu;
		ServerInstance->Modules->DoneWithInterface("HashRequest");
	}

	virtual Version GetVersion()
	{
		// returns the version number of the module to be
		// listed in /MODULES
		return Version("$Id$", VF_COMMON|VF_VENDOR,API_VERSION);
	}

	virtual void OnRehash(User* user)
	{
		cu->DoRehash();
	}

	virtual void OnUserConnect(User* dest)
	{
		std::string* tofree;
		if (dest->GetExt("cloaked_host", tofree))
			return;

		if (dest->host.find('.') != std::string::npos || dest->host.find(':') != std::string::npos)
		{
			unsigned int iv[] = { cu->key1, cu->key2, cu->key3, cu->key4 };
			std::string a = cu->LastTwoDomainParts(dest->host);
			std::string b;

			/* InspIRCd users have two hostnames; A displayed
			 * hostname which can be modified by modules (e.g.
			 * to create vhosts, implement chghost, etc) and a
			 * 'real' hostname which you shouldnt write to.
			 */

			/* 2008/08/18: add <cloak:ipalways> which always cloaks
			 * the IP, for anonymity. --nenolod
			 */
			if (!cu->ipalways)
			{
				/** Reset the Hash module, and send it our IV
				 * We use the default hex table here (for no reason whatsoever)
				 */
				HashResetRequest(this, cu->HashProvider).Send();
				HashKeyRequest(this, cu->HashProvider, iv).Send();

				/* Generate a cloak using specialized Hash */
				std::string hostcloak = cu->prefix + "-" + std::string(HashSumRequest(this, cu->HashProvider, dest->host.c_str()).Send()).substr(0,8) + a;

				/* Fix by brain - if the cloaked host is > the max length of a host (64 bytes
				 * according to the DNS RFC) then tough titty, they get cloaked as an IP.
				 * Their ISP shouldnt go to town on subdomains, or they shouldnt have a kiddie
				 * vhost.
				 */
#ifdef IPV6
				in6_addr testaddr;
				in_addr testaddr2;
				if ((dest->GetProtocolFamily() == AF_INET6) && (inet_pton(AF_INET6,dest->host.c_str(),&testaddr) < 1) && (hostcloak.length() <= 64))
					/* Invalid ipv6 address, and ipv6 user (resolved host) */
					b = hostcloak;
				else if ((dest->GetProtocolFamily() == AF_INET) && (inet_aton(dest->host.c_str(),&testaddr2) < 1) && (hostcloak.length() <= 64))
					/* Invalid ipv4 address, and ipv4 user (resolved host) */
					b = hostcloak;
				else
					/* Valid ipv6 or ipv4 address (not resolved) ipv4 or ipv6 user */
					b = ((!strchr(dest->host.c_str(),':')) ? cu->Cloak4(dest->host.c_str()) : cu->Cloak6(dest->host.c_str()));
#else
				in_addr testaddr;
				if ((inet_aton(dest->host.c_str(),&testaddr) < 1) && (hostcloak.length() <= 64))
					/* Invalid ipv4 address, and ipv4 user (resolved host) */
					b = hostcloak;
				else
					/* Valid ipv4 address (not resolved) ipv4 user */
					b = cu->Cloak4(dest->host.c_str());
#endif
			}
			else
			{
#ifdef IPV6
				if (dest->GetProtocolFamily() == AF_INET6)
					b = cu->Cloak6(dest->GetIPString());
#endif
				if (dest->GetProtocolFamily() == AF_INET)
					b = cu->Cloak4(dest->GetIPString());
			}

			dest->Extend("cloaked_host", new std::string(b));
		}
	}

};

MODULE_INIT(ModuleCloaking)
