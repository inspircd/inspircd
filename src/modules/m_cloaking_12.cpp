/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "hash.h"

/* $ModDesc: Provides masking of user hostnames */

namespace cloak_12
{

enum CloakMode
{
	/** 1.2-compatible host-based cloak */
	MODE_COMPAT_HOST,
	/** 1.2-compatible IP-only cloak */
	MODE_COMPAT_IPONLY
};

/** Handles user mode +x
 */
class CloakUser : public ModeHandler
{
 public:
	LocalStringExt ext;

	std::string debounce_uid;
	time_t debounce_ts;
	int debounce_count;

	CloakUser(Module* source)
		: ModeHandler(source, "cloak_12", 'x', PARAM_NONE, MODETYPE_USER),
		ext(EXTENSIBLE_USER, "cloaked_host", source), debounce_ts(0), debounce_count(0)
	{
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		LocalUser* user = IS_LOCAL(dest);

		/* For remote clients, we don't take any action, we just allow it.
		 * The local server where they are will set their cloak instead.
		 * This is fine, as we will receive it later.
		 */
		if (!user)
		{
			dest->SetMode('x',adding);
			return MODEACTION_ALLOW;
		}

		if (user->uuid == debounce_uid && debounce_ts == ServerInstance->Time())
		{
			// prevent spamming using /mode user +x-x+x-x+x-x
			if (++debounce_count > 2)
				return MODEACTION_DENY;
		}
		else
		{
			debounce_uid = user->uuid;
			debounce_count = 1;
			debounce_ts = ServerInstance->Time();
		}

		if (adding == user->IsModeSet('x'))
			return MODEACTION_DENY;

		/* don't allow this user to spam modechanges */
		if (source == dest)
			user->CommandFloodPenalty += 5000;

		if (adding)
		{
			std::string* cloak = ext.get(user);

			if (!cloak)
			{
				/* Force creation of missing cloak */
				creator->OnUserConnect(user);
				cloak = ext.get(user);
			}
			if (cloak)
			{
				user->ChangeDisplayedHost(cloak->c_str());
				user->SetMode('x',true);
				return MODEACTION_ALLOW;
			}
			else
				return MODEACTION_DENY;
		}
		else
		{
			/* User is removing the mode, so restore their real host
			 * and make it match the displayed one.
			 */
			user->ChangeDisplayedHost(user->host.c_str());
			user->SetMode('x',false);
			return MODEACTION_ALLOW;
		}
	}

};

class CommandCloak : public Command
{
 public:
	CommandCloak(Module* Creator) : Command(Creator, "CLOAK", 1)
	{
		flags_needed = 'o';
		syntax = "<host>";
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user);
};

class ModuleCloaking : public Module
{
 public:
	CloakUser cu;
	CloakMode mode;
	CommandCloak ck;
	std::string prefix;
	unsigned int compatkey[4];
	const char* xtab[4];
	dynamic_reference<HashProvider> Hash;

	ModuleCloaking() : cu(this), mode(MODE_COMPAT_HOST), ck(this), Hash("hash/md5")
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cu);
		ServerInstance->Modules->AddService(ck);
		ServerInstance->Modules->AddService(cu.ext);

		Implementation eventlist[] = { I_OnCheckBan, I_OnUserConnect, I_OnChangeHost };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));

		if (!Hash)
			throw CoreException("Cannot find hash/md5: did you load the m_md5.so module?");
	}

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

	/**
	 * 1.2-style cloaking function
	 * @param table The table to use (will be used mod 4, not a secure parameter)
	 * @param data The item to cloak
	 * @param bytes The number of bytes in the output (output is double this size)
	 */
	std::string sumIV(int table, const std::string& data, int bytes)
	{
		std::string bits = Hash->sum(data, compatkey);
		const char* hex = xtab[table % 4];
		std::string rv;
		for(int i=0; i < bytes; i++)
		{
			unsigned char v = bits[i];
			rv.push_back(hex[v / 16]);
			rv.push_back(hex[v % 16]);
		}
		return rv;
	}

	std::string CompatCloak4(const char* ip)
	{
		irc::sepstream seps(ip, '.');
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

		std::string rv;

		/* Send the Hash module a different hex table for each octet group's Hash sum */
		for (int k = 0; k < 4; k++)
		{
			rv.append(sumIV(compatkey[k]+i[k], octet[k], 3));
			if (k < 3)
				rv.append(".");
		}
		/* Stick them all together */
		return rv;
	}

	std::string CompatCloak6(const char* ip)
	{
		std::vector<std::string> hashies;
		std::string item;
		int rounds = 0;

		/* Reset the Hash module and send it our IV */

		for (const char* input = ip; *input; input++)
		{
			item += *input;
			if (item.length() > 7)
			{
				hashies.push_back(sumIV(compatkey[0]+rounds, item, 4));
				item.clear();
			}
			rounds++;
		}
		if (!item.empty())
		{
			hashies.push_back(sumIV(compatkey[0]+rounds, item, 4));
		}
		/* Stick them all together */
		return irc::stringjoiner(":", hashies, 0, hashies.size() - 1).GetJoined();
	}

	ModResult OnCheckBan(User* user, Channel* chan, const std::string& mask)
	{
		LocalUser* lu = IS_LOCAL(user);
		if (!lu)
			return MOD_RES_PASSTHRU;

		OnUserConnect(lu);
		std::string* cloak = cu.ext.get(user);
		/* Check if they have a cloaked host, but are not using it */
		if (cloak && *cloak != user->dhost)
		{
			char cmask[MAXBUF];
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
	}

	// this unsets umode +x on every host change. If we are actually doing a +x
	// mode change, we will call SetMode back to true AFTER the host change is done.
	void OnChangeHost(User* u, const std::string& host)
	{
		u->SetMode('x', false);
	}

	~ModuleCloaking()
	{
	}

	Version GetVersion()
	{
		std::string testcloak = "broken";
		if (Hash)
		{
			switch (mode)
			{
				case MODE_COMPAT_HOST:
					testcloak = prefix + "-" + sumIV(0, "*", 5);
					break;
				case MODE_COMPAT_IPONLY:
					testcloak = sumIV(0, "*", 5);
					break;
			}
		}
		return Version("Provides masking of user hostnames", VF_COMMON|VF_VENDOR, testcloak);
	}

	void ReadConfig(ConfigReadStatus& stat)
	{
		ConfigTag* tag = ServerInstance->Config->GetTag("cloak");
		prefix = tag->getString("prefix");

		std::string modestr = tag->getString("mode");
		mode = tag->getBool("ipalways") ? MODE_COMPAT_IPONLY : MODE_COMPAT_HOST;
		if (modestr == "compat-host")
			mode = MODE_COMPAT_HOST;
		else if (modestr == "compat-ip")
			mode = MODE_COMPAT_IPONLY;
		else if (modestr != "")
			stat.ReportError(tag, "Bad value for <cloak:mode>; must be compat-host or compat-ip");

		bool lowercase = tag->getBool("lowercase");

		/* These are *not* using the need_positive parameter of ReadInteger -
		 * that will limit the valid values to only the positive values in a
		 * signed int. Instead, accept any value that fits into an int and
		 * cast it to an unsigned int. That will, a bit oddly, give us the full
		 * spectrum of an unsigned integer. - Special
		 *
		 * We must limit the keys or else we get different results on
		 * amd64/x86 boxes. - psychon */
		const unsigned int limit = 0x80000000;
		compatkey[0] = (unsigned int) tag->getInt("key1");
		compatkey[1] = (unsigned int) tag->getInt("key2");
		compatkey[2] = (unsigned int) tag->getInt("key3");
		compatkey[3] = (unsigned int) tag->getInt("key4");

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

		const char* detail = NULL;
		if (!compatkey[0] || compatkey[0] >= limit)
			detail = "<cloak:key1> is not valid, it may be set to a too high/low value, or it may not exist.";
		else if (!compatkey[1] || compatkey[1] >= limit)
			detail = "<cloak:key2> is not valid, it may be set to a too high/low value, or it may not exist.";
		else if (!compatkey[2] || compatkey[2] >= limit)
			detail = "<cloak:key3> is not valid, it may be set to a too high/low value, or it may not exist.";
		else if (!compatkey[3] || compatkey[3] >= limit)
			detail = "<cloak:key4> is not valid, it may be set to a too high/low value, or it may not exist.";

		if (detail)
			stat.ReportError(tag, detail);
	}

	std::string GenCloak(const irc::sockets::sockaddrs& ip, const std::string& ipstr, const std::string& host)
	{
		std::string chost;

		switch (mode)
		{
			case MODE_COMPAT_HOST:
			{
				if (ipstr != host)
				{
					std::string tail = LastTwoDomainParts(host);

					// sumIV is not used here due to a bug in 1.2 cloaking
					chost = prefix + "-" + irc::hex((const unsigned char*)Hash->sum(host, compatkey).data(), 4) + tail;

					/* Fix by brain - if the cloaked host is > the max length of a host (64 bytes
					 * according to the DNS RFC) then they get cloaked as an IP.
					 */
					if (chost.length() <= 64)
						break;
				}
				// fall through to IP cloak
			}
			default:
				if (ip.sa.sa_family == AF_INET6)
					chost = CompatCloak6(ipstr.c_str());
				else
					chost = CompatCloak4(ipstr.c_str());
		}
		return chost;
	}

	void OnUserConnect(LocalUser* dest)
	{
		std::string* cloak = cu.ext.get(dest);
		if (cloak)
			return;

		cu.ext.set(dest, GenCloak(dest->client_sa, dest->GetIPString(), dest->host));
	}
};

CmdResult CommandCloak::Handle(const std::vector<std::string> &parameters, User *user)
{
	ModuleCloaking* mod = (ModuleCloaking*)(Module*)creator;
	irc::sockets::sockaddrs sa;
	std::string cloak;

	if (irc::sockets::aptosa(parameters[0], 0, sa))
		cloak = mod->GenCloak(sa, parameters[0], parameters[0]);
	else
		cloak = mod->GenCloak(sa, "", parameters[0]);

	user->WriteServ("NOTICE %s :*** Cloak for %s is %s", user->nick.c_str(), parameters[0].c_str(), cloak.c_str());

	return CMD_SUCCESS;
}

}

using cloak_12::ModuleCloaking;

MODULE_INIT(ModuleCloaking)
