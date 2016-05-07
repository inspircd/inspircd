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
#include "modules/hash.h"

enum CloakMode
{
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
		ext("cloaked_host", ExtensionItem::EXT_USER, source), debounce_ts(0), debounce_count(0)
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
			dest->SetMode(this, adding);
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

		if (adding == user->IsModeSet(this))
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
				user->ChangeDisplayedHost(*cloak);
				user->SetMode(this, true);
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
			user->SetMode(this, false);
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
	const char* xtab[4];
	dynamic_reference<HashProvider> Hash;

	ModuleCloaking() : cu(this), mode(MODE_OPAQUE), ck(this), Hash(this, "hash/md5")
	{
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

		std::string rv = Hash->GenerateRaw(input).substr(0,len);
		for(int i=0; i < len; i++)
		{
			// this discards 3 bits per byte. We have an
			// overabundance of bits in the hash output, doesn't
			// matter which ones we are discarding.
			rv[i] = base32[rv[i] & 0x1F];
		}
		return rv;
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
			if (ip.sa.sa_family == AF_INET6)
			{
				rv.append(InspIRCd::Format(".%02x%02x.%02x%02x%s",
					ip.in6.sin6_addr.s6_addr[2], ip.in6.sin6_addr.s6_addr[3],
					ip.in6.sin6_addr.s6_addr[0], ip.in6.sin6_addr.s6_addr[1], suffix.c_str()));
			}
			else
			{
				const unsigned char* ip4 = (const unsigned char*)&ip.in4.sin_addr;
				rv.append(InspIRCd::Format(".%d.%d%s", ip4[1], ip4[0], suffix.c_str()));
			}
		}
		return rv;
	}

	ModResult OnCheckBan(User* user, Channel* chan, const std::string& mask) CXX11_OVERRIDE
	{
		LocalUser* lu = IS_LOCAL(user);
		if (!lu)
			return MOD_RES_PASSTHRU;

		OnUserConnect(lu);
		std::string* cloak = cu.ext.get(user);
		/* Check if they have a cloaked host, but are not using it */
		if (cloak && *cloak != user->dhost)
		{
			const std::string cloakMask = user->nick + "!" + user->ident + "@" + *cloak;
			if (InspIRCd::Match(cloakMask, mask))
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
	void OnChangeHost(User* u, const std::string& host) CXX11_OVERRIDE
	{
		if (u->IsModeSet(cu))
		{
			u->SetMode(cu, false);
			u->WriteCommand("MODE", "-" + ConvToStr(cu.GetModeChar()));
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		std::string testcloak = "broken";
		if (Hash)
		{
			switch (mode)
			{
				case MODE_HALF_CLOAK:
					testcloak = prefix + SegmentCloak("*", 3, 8) + suffix;
					break;
				case MODE_OPAQUE:
					testcloak = prefix + SegmentCloak("*", 4, 8) + suffix;
			}
		}
		return Version("Provides masking of user hostnames", VF_COMMON|VF_VENDOR, testcloak);
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("cloak");
		prefix = tag->getString("prefix");
		suffix = tag->getString("suffix", ".IP");

		std::string modestr = tag->getString("mode");
		if (modestr == "half")
			mode = MODE_HALF_CLOAK;
		else if (modestr == "full")
			mode = MODE_OPAQUE;
		else
			throw ModuleException("Bad value for <cloak:mode>; must be half or full");

		key = tag->getString("key");
		if (key.empty() || key == "secret")
			throw ModuleException("You have not defined cloak keys for m_cloaking. Define <cloak:key> as a network-wide secret.");
	}

	std::string GenCloak(const irc::sockets::sockaddrs& ip, const std::string& ipstr, const std::string& host)
	{
		std::string chost;

		switch (mode)
		{
			case MODE_HALF_CLOAK:
			{
				if (ipstr != host)
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

	void OnUserConnect(LocalUser* dest) CXX11_OVERRIDE
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

	user->WriteNotice("*** Cloak for " + parameters[0] + " is " + cloak);

	return CMD_SUCCESS;
}

MODULE_INIT(ModuleCloaking)
