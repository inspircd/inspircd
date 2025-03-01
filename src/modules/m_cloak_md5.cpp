/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2017 Sheogorath <sheogorath@shivering-isles.com>
 *   Copyright (C) 2016 Adam <Adam@anope.org>
 *   Copyright (C) 2013, 2017-2019, 2021-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007-2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007 John Brooks <john@jbrooks.io>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <om@inspircd.org>
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
#include "modules/cloak.h"
#include "modules/hash.h"

enum CloakMode
{
	/** 2.0 cloak of "half" of the hostname plus the full IP hash */
	MODE_HALF_CLOAK,

	/** 2.0 cloak of IP hash, split at 2 common CIDR range points */
	MODE_OPAQUE
};

// lowercase-only encoding similar to base64, used for hash output
static constexpr char base32[] = "0123456789abcdefghijklmnopqrstuv";

// The minimum length of a cloak key.
static constexpr size_t minkeylen = 30;

struct CloakInfo final
	: public Cloak::Method
{
	// The method used for cloaking users.
	CloakMode mode;

	// The number of parts of the hostname shown when using half cloaking.
	unsigned int domainparts;

	// Whether to ignore the case of a hostname when cloaking it.
	bool ignorecase;

	// The secret used for generating cloaks.
	std::string key;

	// Dynamic reference to the md5 implementation.
	dynamic_reference_nocheck<HashProvider> md5;

	// The prefix for cloaks (e.g. MyNet-).
	std::string prefix;

	// The suffix for IP cloaks (e.g. .IP).
	std::string suffix;

	CloakInfo(const Cloak::Engine* engine, const std::shared_ptr<ConfigTag>& tag, CloakMode Mode, const std::string& Key)
		: Cloak::Method(engine, tag)
		, mode(Mode)
		, domainparts(tag->getNum<unsigned int>("domainparts", 3, 1, 10))
		, ignorecase(tag->getBool("ignorecase"))
		, key(Key)
		, md5(engine->creator, "hash/md5")
		, prefix(tag->getString("prefix"))
		, suffix(tag->getString("suffix", ".IP"))
	{
	}

	/*
	 * 2.0-style cloaking function
	 * @param item The item to cloak (part of an IP or hostname)
	 * @param id A unique ID for this type of item (to make it unique if the item matches)
	 * @param len The length of the output. Maximum for MD5 is 16 characters.
	 */
	std::string SegmentCloak(const std::string& item, char id, size_t len)
	{
		std::string input;
		input.reserve(key.length() + 3 + item.length());
		input.append(1, id);
		input.append(key);
		input.append(1, '\0'); // null does not terminate a C++ string
		if (ignorecase)
			std::transform(item.begin(), item.end(), std::back_inserter(input), ::tolower);
		else
			input.append(item);

		std::string rv = md5->GenerateRaw(input).substr(0, len);
		for(size_t i = 0; i < len; i++)
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
		size_t hop1;
		size_t hop2;
		size_t hop3;
		size_t len1;
		size_t len2;
		std::string rv;
		if (ip.family() == AF_INET6)
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
			if (ip.family() == AF_INET6)
			{
				rv.append(INSP_FORMAT(".{:02x}{:02x}.{:02x}{:02x}{}",
					ip.in6.sin6_addr.s6_addr[2], ip.in6.sin6_addr.s6_addr[3],
					ip.in6.sin6_addr.s6_addr[0], ip.in6.sin6_addr.s6_addr[1],
					suffix));
			}
			else
			{
				const unsigned char* ip4 = (const unsigned char*)&ip.in4.sin_addr;
				rv.append(INSP_FORMAT(".{}.{}{}", ip4[1], ip4[0], suffix));
			}
		}
		return rv;
	}

	std::string GetCompatLinkData()
	{
		std::string data = "broken";
		if (md5)
		{
			switch (mode)
			{
				case MODE_HALF_CLOAK:
					// Use old cloaking verification to stay compatible with 2.0
					// But verify domainparts and ignorecase when use 3.0-only features
					if (domainparts == 3 && !ignorecase)
						data = prefix + SegmentCloak("*", 3, 8) + suffix;
					else
					{
						irc::sockets::sockaddrs sa;
						data = GenCloak(sa, "", data + ConvToStr(domainparts)) + (ignorecase ? "-ci" : "");
					}
					break;
				case MODE_OPAQUE:
					data = prefix + SegmentCloak("*", 4, 8) + suffix + (ignorecase ? "-ci" : "");
			}
		}
		return data;
	}

	void GetLinkData(Module::LinkData& data, std::string& compatdata) override
	{
		data["domain-parts"] = ConvToStr(domainparts);
		data["ignore-case"] = ignorecase ? "yes" : "no";
		data["prefix"] = prefix;
		data["suffix"] = suffix;

		// IMPORTANT: link data is sent over unauthenticated server links so we
		// can't directly send the key here. Instead we use dummy cloaks that
		// allow verification of or less the same thing.
		const std::string broken = "missing-md5-module";
		data["cloak-v4"]   = md5 ? Generate("123.123.123.123")                        : broken;
		data["cloak-v6"]   = md5 ? Generate("dead:beef:cafe::")                       : broken;
		data["cloak-host"] = md5 ? Generate("extremely.long.inspircd.cloak.example")  : broken;

		compatdata = GetCompatLinkData();
	}

	bool IsLinkSensitive() const override
	{
		// This method always wants to be at the front.
		return true;
	}

	std::string GenCloak(const irc::sockets::sockaddrs& ip, const std::string& ipstr, const std::string& host)
	{
		std::string chost;

		irc::sockets::sockaddrs hostip(false);
		bool host_is_ip = hostip.from_ip_port(host, ip.port()) && hostip == ip;

		switch (mode)
		{
			case MODE_HALF_CLOAK:
			{
				if (!host_is_ip)
					chost = prefix + SegmentCloak(host, 1, 6) + "." + Cloak::VisiblePart(host, domainparts, '.');
				if (chost.empty() || chost.length() > 50)
					chost = SegmentIP(ip, false);
				break;
			}
			case MODE_OPAQUE:
				chost = SegmentIP(ip, true);
				break;
		}
		return chost;
	}

	std::string Generate(LocalUser* user) override ATTR_NOT_NULL(2)
	{
		if (!md5 || !user->client_sa.is_ip() || !MatchesUser(user))
			return {};

		return GenCloak(user->client_sa, user->GetAddress(), user->GetRealHost());
	}

	std::string Generate(const std::string& hostip) override
	{
		if (!md5)
			return {};

		irc::sockets::sockaddrs sa;
		const char* ipaddr = sa.from_ip(hostip) ? hostip.c_str() : "";
		return GenCloak(sa, ipaddr, hostip);
	}
};

class MD5Engine final
	: public Cloak::Engine
{
private:
	// Dynamic reference to the md5 implementation.
	dynamic_reference_nocheck<HashProvider> md5;

	// The method used for cloaking users.
	CloakMode mode;

public:
	MD5Engine(Module* Creator, const std::string& Name, CloakMode cm)
		: Cloak::Engine(Creator, Name)
		, md5(Creator, "hash/md5")
		, mode(cm)
	{
	}

	Cloak::MethodPtr Create(const std::shared_ptr<ConfigTag>& tag, bool primary) override
	{
		// Check any dependent modules are loaded.
		if (!md5)
			throw ModuleException(creator, "Unable to create a " + name.substr(6) + " cloak without the md5 module, at" + tag->source.str());

		// Ensure that we have the <cloak:key> parameter.
		const std::string key = tag->getString("key");
		if (key.empty())
			throw ModuleException(creator, INSP_FORMAT("You have not defined a cloaking key. Define <cloak:key> as a {}+ character network-wide secret, at {}", minkeylen, tag->source.str()));

		// If we are the first cloak method then mandate a strong key.
		if (primary)
		{
			if (key.length() < minkeylen)
				throw ModuleException(creator, INSP_FORMAT("Your cloaking key is not secure. It should be at least {} characters long, at {}", minkeylen, tag->source.str()));

			ServerInstance->Logs.Normal(MODNAME, "The {} cloak method is deprecated and will be removed in the next major version of InspIRCd. Consider migrating to cloak_sha256 instead. See " INSPIRCD_DOCS "modules/cloak_md5 for more information.",
				name.c_str() + 6);
		}

		return std::make_shared<CloakInfo>(this, tag, mode, key);
	}
};

class ModuleCloakMD5 final
	: public Module
{
private:
	MD5Engine halfcloak;
	MD5Engine fullcloak;

public:
	ModuleCloakMD5()
		: Module(VF_VENDOR, "Adds the half and full cloaking methods for use with the cloak module.")
		, halfcloak(this, "half", MODE_HALF_CLOAK)
		, fullcloak(this, "full", MODE_OPAQUE)
	{
	}
};

MODULE_INIT(ModuleCloakMD5)
