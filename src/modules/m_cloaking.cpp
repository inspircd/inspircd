/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
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
	Module* HashProvider;
	const char *xtab[4];
	LocalStringExt ext;

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
			return host;
		else
			return host.substr(splitdot);
	}

	CloakUser(InspIRCd* Instance, Module* source, Module* Hash) 
		: ModeHandler(source, 'x', PARAM_NONE, MODETYPE_USER), HashProvider(Hash),
		ext("cloaked_host", source)
	{
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
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

		/* don't allow this user to spam modechanges */
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

				std::string* cloak = ext.get(dest);

				if (!cloak)
				{
					/* Force creation of missing cloak */
					creator->OnUserConnect(dest);
					cloak = ext.get(dest);
				}
				if (cloak)
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
		HashResetRequest(creator, HashProvider).Send();
		HashKeyRequest(creator, HashProvider, iv).Send();

		/* Send the Hash module a different hex table for each octet group's Hash sum */
		for (int k = 0; k < 4; k++)
		{
			HashHexRequest(creator, HashProvider, xtab[(iv[k]+i[k]) % 4]).Send();
			ra[k] = std::string(HashSumRequest(creator, HashProvider, octet[k]).Send()).substr(0,6);
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
		HashResetRequest(creator, HashProvider).Send();
		HashKeyRequest(creator, HashProvider, iv).Send();

		for (const char* input = ip; *input; input++)
		{
			item += *input;
			if (item.length() > 7)
			{
				/* Send the Hash module a different hex table for each octet group's Hash sum */
				HashHexRequest(creator, HashProvider, xtab[(key1+rounds) % 4]).Send();
				hashies.push_back(std::string(HashSumRequest(creator, HashProvider, item).Send()).substr(0,8));
				item.clear();
			}
			rounds++;
		}
		if (!item.empty())
		{
			/* Send the Hash module a different hex table for each octet group's Hash sum */
			HashHexRequest(creator, HashProvider, xtab[(key1+rounds) % 4]).Send();
			hashies.push_back(std::string(HashSumRequest(creator, HashProvider, item).Send()).substr(0,8));
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

 public:
	ModuleCloaking(InspIRCd* Me)
		: Module(Me)
	{
		/* Attempt to locate the md5 service provider, bail if we can't find it */
		Module* HashModule = ServerInstance->Modules->Find("m_md5.so");
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
		Extensible::Register(&cu->ext);

		Implementation eventlist[] = { I_OnRehash, I_OnCheckBan, I_OnUserConnect };
		ServerInstance->Modules->Attach(eventlist, this, 3);

		CloakExistingUsers();
	}

	void CloakExistingUsers()
	{
		std::string* cloak;
		for (std::vector<User*>::iterator u = ServerInstance->Users->local_users.begin(); u != ServerInstance->Users->local_users.end(); u++)
		{
			cloak = cu->ext.get(*u);
			if (!cloak)
			{
				OnUserConnect(*u);
			}
		}
	}

	ModResult OnCheckBan(User* user, Channel* chan, const std::string& mask)
	{
		char cmask[MAXBUF];
		std::string* cloak = cu->ext.get(user);
		/* Check if they have a cloaked host, but are not using it */
		if (cloak && *cloak != user->dhost)
		{
			snprintf(cmask, MAXBUF, "%s!%s@%s", user->nick.c_str(), user->ident.c_str(), cloak->c_str());
			if (InspIRCd::Match(cmask,mask))
				return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

 	void Prioritize()
	{
		/* Needs to be after m_banexception etc. */
		ServerInstance->Modules->SetPriority(this, I_OnCheckBan, PRIORITY_LAST);

		/* but before m_conn_umodes, so host is generated ready to apply */
		Module *um = ServerInstance->Modules->Find("m_conn_umodes.so");
		ServerInstance->Modules->SetPriority(this, I_OnUserConnect, PRIORITY_AFTER, &um);
	}

	~ModuleCloaking()
	{
		ServerInstance->Modes->DelMode(cu);
		delete cu;
		ServerInstance->Modules->DoneWithInterface("HashRequest");
	}

	Version GetVersion()
	{
		// returns the version number of the module to be
		// listed in /MODULES
		return Version("$Id$", VF_COMMON|VF_VENDOR,API_VERSION);
	}

	void OnRehash(User* user)
	{
		cu->DoRehash();
	}

	void OnUserConnect(User* dest)
	{
		std::string* cloak = cu->ext.get(dest);
		if (cloak)
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
				/** Reset the Hash module, and send it our IV and hex table */
				HashResetRequest(this, cu->HashProvider).Send();
				HashKeyRequest(this, cu->HashProvider, iv).Send();
				HashHexRequest(this, cu->HashProvider, cu->xtab[(dest->host[0]) % 4]);

				/* Generate a cloak using specialized Hash */
				std::string hostcloak = cu->prefix + "-" + std::string(HashSumRequest(this, cu->HashProvider, dest->host.c_str()).Send()).substr(0,8) + a;

				/* Fix by brain - if the cloaked host is > the max length of a host (64 bytes
				 * according to the DNS RFC) then tough titty, they get cloaked as an IP.
				 * Their ISP shouldnt go to town on subdomains, or they shouldnt have a kiddie
				 * vhost.
				 */
				std::string testaddr;
				int testport;
				if (!irc::sockets::satoap(&dest->client_sa, testaddr, testport) && (hostcloak.length() <= 64))
					/* not a valid address, must have been a host, so cloak as a host */
					b = hostcloak;
				else if (dest->client_sa.sa.sa_family == AF_INET6)
					b = cu->Cloak6(dest->GetIPString());
				else
					b = cu->Cloak4(dest->GetIPString());
			}
			else
			{
				if (dest->client_sa.sa.sa_family == AF_INET6)
					b = cu->Cloak6(dest->GetIPString());
				else
					b = cu->Cloak4(dest->GetIPString());
			}

			cu->ext.set(dest,b);
		}
	}

};

MODULE_INIT(ModuleCloaking)
