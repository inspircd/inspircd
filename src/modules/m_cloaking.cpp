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

enum CloakMode
{
	/** 1.2-compatible host-based cloak */
	MODE_COMPAT_HOST,
	/** 1.2-compatible IP-only cloak */
	MODE_COMPAT_IPONLY,
	/** 2.0 cloak of "half" of the hostname plus the full IP hash */
	MODE_HALF_CLOAK,
	/** 2.0 cloak of IP hash, split at 2 common CIDR range points */
	MODE_OPAQUE
};

// lowercase-only encoding similar to base64, used for hash output
static const char base32[] = "0123456789abcdefghijklmnopqrstuv";

/** Handles user mode +x
 */
class CloakUser : public ModeHandler
{
 public:
	LocalStringExt ext;

	CloakUser(Module* source)
		: ModeHandler(source, "cloak", 'x', PARAM_NONE, MODETYPE_USER),
		ext("cloaked_host", source)
	{
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		/* For remote clients, we don't take any action, we just allow it.
		 * The local server where they are will set their cloak instead.
		 * This is fine, as we will receive it later.
		 */
		if (!IS_LOCAL(dest))
		{
			dest->SetMode('x',adding);
			return MODEACTION_ALLOW;
		}

		/* don't allow this user to spam modechanges */
		IS_LOCAL(dest)->CommandFloodPenalty += 5000;

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

				if (!cloak && IS_LOCAL(dest))
				{
					/* Force creation of missing cloak */
					creator->OnUserConnect(IS_LOCAL(dest));
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

};


class ModuleCloaking : public Module
{
 private:
	CloakUser cu;
	CloakMode mode;
	std::string prefix;
	std::string key;
	unsigned int compatkey[4];
	const char* xtab[4];
	dynamic_reference<HashProvider> Hash;

 public:
	ModuleCloaking() : cu(this), Hash(this, "hash/md5")
	{
	}

	void init()
	{
		OnRehash(NULL);

		/* Register it with the core */
		ServerInstance->Modules->AddService(cu);

		ServerInstance->Extensions.Register(&cu.ext);

		Implementation eventlist[] = { I_OnRehash, I_OnCheckBan, I_OnUserConnect };
		ServerInstance->Modules->Attach(eventlist, this, 3);
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
			return host;
		else
			return host.substr(splitdot);
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
			rv.append(Hash->sumIV(compatkey, xtab[(compatkey[k]+i[k]) % 4], octet[k]).substr(0,6));
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
				hashies.push_back(Hash->sumIV(compatkey, xtab[(compatkey[1]+rounds) % 4], item).substr(0,8));
				item.clear();
			}
			rounds++;
		}
		if (!item.empty())
		{
			hashies.push_back(Hash->sumIV(compatkey, xtab[(compatkey[1]+rounds) % 4], item).substr(0,8));
		}
		/* Stick them all together */
		return irc::stringjoiner(":", hashies, 0, hashies.size() - 1).GetJoined();
	}

	std::string SegmentIP(const irc::sockets::sockaddrs& ip, bool full)
	{
		std::string bindata;
		int hop1, hop2, hop3;
		std::string rv;
		if (ip.sa.sa_family == AF_INET6)
		{
			bindata = std::string((const char*)ip.in6.sin6_addr.s6_addr, 16);
			hop1 = 8;
			hop2 = 6;
			hop3 = 4;
			rv.reserve(prefix.length() + 37);
		}
		else
		{
			bindata = std::string((const char*)&ip.in4.sin_addr, 4);
			hop1 = 3;
			hop2 = 0;
			hop3 = 2;
			rv.reserve(prefix.length() + 30);
		}

		rv.append(prefix);
		rv.append(SegmentCloak(bindata, 10));
		rv.append(1, '.');
		bindata.erase(hop1);
		rv.append(SegmentCloak(bindata, 11));
		if (hop2)
		{
			rv.append(1, '.');
			bindata.erase(hop2);
			rv.append(SegmentCloak(bindata, 12));
		}

		if (full)
		{
			rv.append(1, '.');
			bindata.erase(hop3);
			rv.append(SegmentCloak(bindata, 13));
			rv.append(".IP");
		}
		else
		{
			char buf[50];
			if (ip.sa.sa_family == AF_INET6)
			{
				snprintf(buf, 50, ".%02x%02x.%02x%02x.IP",
					ip.in6.sin6_addr.s6_addr[2], ip.in6.sin6_addr.s6_addr[3],
					ip.in6.sin6_addr.s6_addr[0], ip.in6.sin6_addr.s6_addr[1]);
			}
			else
			{
				const unsigned char* ip4 = (const unsigned char*)&ip.in4.sin_addr;
				snprintf(buf, 50, ".%d.%d.IP", ip4[1], ip4[0]);
			}
			rv.append(buf);
		}
		return rv;
	}

	std::string SegmentCloak(const std::string& item, char id)
	{
		std::string input;
		input.reserve(key.length() + 3 + item.length());
		input.append(1, id);
		input.append(key);
		input.append(1, 0); // null does not terminate a C++ string
		input.append(item);

		std::string rv = Hash->sum(input).substr(0,6);
		for(int i=0; i < 6; i++)
		{
			// this discards 3 bits per byte. We have an
			// overabundance of bits in the hash output, doesn't
			// matter which ones we are discarding.
			rv[i] = base32[rv[i] & 0x1F];
		}
		return rv;
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
					testcloak = prefix + "-" + Hash->sumIV(compatkey, xtab[0], "*").substr(0,10);
					break;
				case MODE_COMPAT_IPONLY:
					testcloak = Hash->sumIV(compatkey, xtab[0], "*").substr(0,10);
					break;
				case MODE_HALF_CLOAK:
					testcloak = prefix + SegmentCloak("*", 3);
					break;
				case MODE_OPAQUE:
					testcloak = prefix + SegmentCloak("*", 4);
			}
		}
		return Version("Provides masking of user hostnames", VF_COMMON|VF_VENDOR, testcloak);
	}

	void OnRehash(User* user)
	{
		ConfigReader Conf;
		prefix = Conf.ReadValue("cloak","prefix",0);

		std::string modestr = Conf.ReadValue("cloak", "mode", 0);
		if (modestr == "compat-host")
			mode = MODE_COMPAT_HOST;
		else if (modestr == "compat-ip")
			mode = MODE_COMPAT_IPONLY;
		else if (modestr == "half")
			mode = MODE_HALF_CLOAK;
		else if (modestr == "full")
			mode = MODE_OPAQUE;
		else
			throw ModuleException("Bad value for <cloak:mode>; must be one of compat-host, compat-ip, half, full");

		if (mode == MODE_COMPAT_HOST || mode == MODE_COMPAT_IPONLY)
		{
			bool lowercase = Conf.ReadFlag("cloak", "lowercase", 0);

			/* These are *not* using the need_positive parameter of ReadInteger -
			 * that will limit the valid values to only the positive values in a
			 * signed int. Instead, accept any value that fits into an int and
			 * cast it to an unsigned int. That will, a bit oddly, give us the full
			 * spectrum of an unsigned integer. - Special
			 *
			 * We must limit the keys or else we get different results on
			 * amd64/x86 boxes. - psychon */
			const unsigned int limit = 0x80000000;
			compatkey[0] = (unsigned int) Conf.ReadInteger("cloak","key1",0,false);
			compatkey[1] = (unsigned int) Conf.ReadInteger("cloak","key2",0,false);
			compatkey[2] = (unsigned int) Conf.ReadInteger("cloak","key3",0,false);
			compatkey[3] = (unsigned int) Conf.ReadInteger("cloak","key4",0,false);

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

			if (!compatkey[0] || !compatkey[1] || !compatkey[2] || !compatkey[3] ||
				compatkey[0] >= limit || compatkey[1] >= limit || compatkey[2] >= limit || compatkey[3] >= limit)
			{
				std::string detail;
				if (!compatkey[0] || compatkey[0] >= limit)
					detail = "<cloak:key1> is not valid, it may be set to a too high/low value, or it may not exist.";
				else if (!compatkey[1] || compatkey[1] >= limit)
					detail = "<cloak:key2> is not valid, it may be set to a too high/low value, or it may not exist.";
				else if (!compatkey[2] || compatkey[2] >= limit)
					detail = "<cloak:key3> is not valid, it may be set to a too high/low value, or it may not exist.";
				else if (!compatkey[3] || compatkey[3] >= limit)
					detail = "<cloak:key4> is not valid, it may be set to a too high/low value, or it may not exist.";

				throw ModuleException("You have not defined cloak keys for m_cloaking!!! THIS IS INSECURE AND SHOULD BE CHECKED! - " + detail);
			}
		}
		else
		{
			key = Conf.ReadFlag("cloak", "key", 0);
			if (key.empty() || key == "secret")
				throw ModuleException("You have not defined cloak keys for m_cloaking. Define <cloak:key> as a network-wide secret.");
		}
	}

	void OnUserConnect(LocalUser* dest)
	{
		std::string* cloak = cu.ext.get(dest);
		if (cloak)
			return;

		std::string ipstr = dest->GetIPString();
		std::string chost;

		switch (mode)
		{
			case MODE_COMPAT_HOST:
			{
				if (ipstr != dest->host)
				{
					std::string tail = LastTwoDomainParts(dest->host);

					/* Generate a cloak using specialized Hash */
					chost = prefix + "-" + Hash->sumIV(compatkey, xtab[(dest->host[0]) % 4], dest->host).substr(0,8) + tail;

					/* Fix by brain - if the cloaked host is > the max length of a host (64 bytes
					 * according to the DNS RFC) then they get cloaked as an IP.
					 */
					if (chost.length() <= 64)
						break;
				}
				// fall through to IP cloak
			}
			case MODE_COMPAT_IPONLY:
				if (dest->client_sa.sa.sa_family == AF_INET6)
					chost = CompatCloak6(ipstr.c_str());
				else
					chost = CompatCloak4(ipstr.c_str());
				break;
			case MODE_HALF_CLOAK:
			{
				if (ipstr != dest->host)
					chost = prefix + SegmentCloak(dest->host, 1) + LastTwoDomainParts(dest->host);
				if (chost.empty() || chost.length() > 50)
					chost = SegmentIP(dest->client_sa, false);
				break;
			}
			case MODE_OPAQUE:
			default:
				chost = SegmentIP(dest->client_sa, true);
		}
		cu.ext.set(dest,chost);
	}

};

MODULE_INIT(ModuleCloaking)
