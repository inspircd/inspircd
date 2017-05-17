/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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

	std::string debounce_uid;
	time_t debounce_ts;
	int debounce_count;

	CloakUser(Module* source)
		: ModeHandler(source, "cloak", 'x', PARAM_NONE, MODETYPE_USER),
		ext("cloaked_host", source), debounce_ts(0), debounce_count(0)
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
			// assume this is more correct
			if (user->registered != REG_ALL && user->host != user->dhost)
				return MODEACTION_DENY;

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
			user->SetMode('x',false);
			user->ChangeDisplayedHost(user->host.c_str());
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
	std::string suffix;
	std::string key;
	unsigned int compatkey[4];
	const char* xtab[4];
	dynamic_reference<HashProvider> Hash;

	ModuleCloaking() : cu(this), mode(MODE_OPAQUE), ck(this), Hash(this, "hash/md5")
	{
	}

	void init()
	{
		OnRehash(NULL);

		ServerInstance->Modules->AddService(cu);
		ServerInstance->Modules->AddService(ck);
		ServerInstance->Modules->AddService(cu.ext);

		Implementation eventlist[] = { I_OnRehash, I_OnCheckBan, I_OnUserConnect, I_OnChangeHost };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
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
	 * 2.0-style cloaking function
	 * @param item The item to cloak (part of an IP or hostname)
	 * @param id A unique ID for this type of item (to make it unique if the item matches)
	 * @param len The length of the output. Maximum for MD5 is 16 characters.
	 */
	std::string SegmentCloak(const std::string& item, char id, int len)
	{
		std::string input;
		input.reserve(key.length() + 3 + item.length());
		input.append(1, id);
		input.append(key);
		input.append(1, '\0'); // null does not terminate a C++ string
		input.append(item);

		std::string rv = Hash->sum(input).substr(0,len);
		for(int i=0; i < len; i++)
		{
			// this discards 3 bits per byte. We have an
			// overabundance of bits in the hash output, doesn't
			// matter which ones we are discarding.
			rv[i] = base32[rv[i] & 0x1F];
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
				hashies.push_back(Hash->sumIV(compatkey, xtab[(compatkey[0]+rounds) % 4], item).substr(0,8));
				item.clear();
			}
			rounds++;
		}
		if (!item.empty())
		{
			hashies.push_back(Hash->sumIV(compatkey, xtab[(compatkey[0]+rounds) % 4], item).substr(0,8));
		}
		/* Stick them all together */
		return irc::stringjoiner(":", hashies, 0, hashies.size() - 1).GetJoined();
	}

	std::string SegmentIP(const irc::sockets::sockaddrs& ip, bool full)
	{
		std::string bindata;
		int hop1, hop2, hop3;
		int len1, len2;
		std::string rv;
		if (ip.sa.sa_family == AF_INET6)
		{
			bindata = std::string((const char*)ip.in6.sin6_addr.s6_addr, 16);
			hop1 = 8;
			hop2 = 6;
			hop3 = 4;
			len1 = 6;
			len2 = 4;
			// pfx s1.s2.s3. (xxxx.xxxx or s4) sfx
			//     6  4  4    9/6
			rv.reserve(prefix.length() + 26 + suffix.length());
		}
		else
		{
			bindata = std::string((const char*)&ip.in4.sin_addr, 4);
			hop1 = 3;
			hop2 = 0;
			hop3 = 2;
			len1 = len2 = 3;
			// pfx s1.s2. (xxx.xxx or s3) sfx
			rv.reserve(prefix.length() + 15 + suffix.length());
		}

		rv.append(prefix);
		rv.append(SegmentCloak(bindata, 10, len1));
		rv.append(1, '.');
		bindata.erase(hop1);
		rv.append(SegmentCloak(bindata, 11, len2));
		if (hop2)
		{
			rv.append(1, '.');
			bindata.erase(hop2);
			rv.append(SegmentCloak(bindata, 12, len2));
		}

		if (full)
		{
			rv.append(1, '.');
			bindata.erase(hop3);
			rv.append(SegmentCloak(bindata, 13, 6));
			rv.append(suffix);
		}
		else
		{
			char buf[50];
			if (ip.sa.sa_family == AF_INET6)
			{
				snprintf(buf, 50, ".%02x%02x.%02x%02x%s",
					ip.in6.sin6_addr.s6_addr[2], ip.in6.sin6_addr.s6_addr[3],
					ip.in6.sin6_addr.s6_addr[0], ip.in6.sin6_addr.s6_addr[1], suffix.c_str());
			}
			else
			{
				const unsigned char* ip4 = (const unsigned char*)&ip.in4.sin_addr;
				snprintf(buf, 50, ".%d.%d%s", ip4[1], ip4[0], suffix.c_str());
			}
			rv.append(buf);
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

	// this unsets umode +x on every host change. If we are actually doing a +x
	// mode change, we will call SetMode back to true AFTER the host change is done.
	void OnChangeHost(User* u, const std::string& host)
	{
		if(u->IsModeSet('x'))
		{
			u->SetMode('x', false);
			u->WriteServ("MODE %s -x", u->nick.c_str());
		}
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
					testcloak = prefix + SegmentCloak("*", 3, 8) + suffix;
					break;
				case MODE_OPAQUE:
					testcloak = prefix + SegmentCloak("*", 4, 8) + suffix;
			}
		}
		return Version("Provides masking of user hostnames", VF_COMMON|VF_VENDOR, testcloak);
	}

	void OnRehash(User* user)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("cloak");
		prefix = tag->getString("prefix");
		suffix = tag->getString("suffix", ".IP");

		std::string modestr = tag->getString("mode");
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
			key = tag->getString("key");
			if (key.empty() || key == "secret")
				throw ModuleException("You have not defined cloak keys for m_cloaking. Define <cloak:key> as a network-wide secret.");
		}
	}

	std::string GenCloak(const irc::sockets::sockaddrs& ip, const std::string& ipstr, const std::string& host)
	{
		std::string chost;

		irc::sockets::sockaddrs hostip;
		bool host_is_ip = irc::sockets::aptosa(host, ip.port(), hostip) && hostip == ip;

		switch (mode)
		{
			case MODE_COMPAT_HOST:
			{
				if (!host_is_ip)
				{
					std::string tail = LastTwoDomainParts(host);

					// xtab is not used here due to a bug in 1.2 cloaking
					chost = prefix + "-" + Hash->sumIV(compatkey, "0123456789abcdef", host).substr(0,8) + tail;

					/* Fix by brain - if the cloaked host is > the max length of a host (64 bytes
					 * according to the DNS RFC) then they get cloaked as an IP.
					 */
					if (chost.length() <= 64)
						break;
				}
				// fall through to IP cloak
			}
			case MODE_COMPAT_IPONLY:
				if (ip.sa.sa_family == AF_INET6)
					chost = CompatCloak6(ipstr.c_str());
				else
					chost = CompatCloak4(ipstr.c_str());
				break;
			case MODE_HALF_CLOAK:
			{
				if (!host_is_ip)
					chost = prefix + SegmentCloak(host, 1, 6) + LastTwoDomainParts(host);
				if (chost.empty() || chost.length() > 50)
					chost = SegmentIP(ip, false);
				break;
			}
			case MODE_OPAQUE:
			default:
				chost = SegmentIP(ip, true);
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

MODULE_INIT(ModuleCloaking)
