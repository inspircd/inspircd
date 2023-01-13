/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2017 Adam <Adam@anope.org>
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

class SHA256Method final
	: public Cloak::Method
{
private:
	// The base32 table used for upper-case cloaks.
	static constexpr unsigned char base32upper[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

	// The base32 table used for lower-case cloaks.
	static constexpr unsigned char base32lower[] = "abcdefghijklmnopqrstuvwxyz234567";

	// The number of bytes of the hash to use when downsampling.
	static constexpr size_t segmentlen = 8;

	// The number of parts of the hostname shown.
	unsigned long hostparts;

	// The secret used for generating cloaks.
	std::string key;

	// The prefix for cloaks (e.g. MyNet).
	std::string prefix;

	// Dynamic reference to the sha256 implementation.
	dynamic_reference_nocheck<HashProvider> sha256;

	// The base32 table used when encoding.
	const unsigned char* table;

	// The suffix for IP cloaks (e.g. IP).
	std::string suffix;

	std::string CloakAddress(const irc::sockets::sockaddrs& sa)
	{
		switch (sa.family())
		{
			case AF_INET:
				return CloakIPv4(sa.in4.sin_addr.s_addr);
			case AF_INET6:
				return CloakIPv6(sa.in6.sin6_addr.s6_addr);
			case AF_UNIX:
				return CloakHost(sa.un.sun_path, '/');
		}

		// Should never be reached.
		return {};
	}

	std::string CloakIPv4(unsigned long address)
	{
		// IPv4 addresses are cloaked in the form ALPHA.BETA.GAMMA
		//
		// ALPHA is unique for a.b.c.d
		// BETA  is unique for a.b.c
		// GAMMA is unique for a.b
		unsigned int a = (unsigned int)(address)       & 0xFF;
		unsigned int b = (unsigned int)(address >> 8)  & 0xFF;
		unsigned int c = (unsigned int)(address >> 16) & 0xFF;
		unsigned int d = (unsigned int)(address >> 24) & 0xFF;

		const std::string alpha = Hash(InspIRCd::Format("%u.%u.%u.%u", a, b, c, d));
		const std::string beta  = Hash(InspIRCd::Format("%u.%u.%u", a, b, c));
		const std::string gamma = Hash(InspIRCd::Format("%u.%u", a, b));

		return Wrap(InspIRCd::Format("%s.%s.%s", alpha.c_str(), beta.c_str(), gamma.c_str()), ".");
	}

	std::string CloakIPv6(const unsigned char* address)
	{
		// IPv6 addresses are cloaked in the form ALPHA.BETA.GAMMA.IP
		//
		// ALPHA is unique for a:b:c:d:e:f:g:h
		// BETA  is unique for a:b:c:d:e:f:g
		// GAMMA is unique for a:b:c:d
		const uint16_t* address16 = reinterpret_cast<const uint16_t*>(address);
		unsigned int a = ntohs(address16[0]);
		unsigned int b = ntohs(address16[1]);
		unsigned int c = ntohs(address16[2]);
		unsigned int d = ntohs(address16[3]);
		unsigned int e = ntohs(address16[4]);
		unsigned int f = ntohs(address16[5]);
		unsigned int g = ntohs(address16[6]);
		unsigned int h = ntohs(address16[7]);

		const std::string alpha = Hash(InspIRCd::Format("%x:%x:%x:%x:%x:%x:%x:%x", a, b, c, d, e, f, g, h));
		const std::string beta  = Hash(InspIRCd::Format("%x:%x:%x:%x:%x:%x:%x", a, b, c, d, e, f, g));
		const std::string gamma = Hash(InspIRCd::Format("%x:%x:%x:%x", a, b, c, d));

		return Wrap(InspIRCd::Format("%s:%s:%s", alpha.c_str(), beta.c_str(), gamma.c_str()), ":");
	}

	std::string CloakHost(const std::string& host, char separator)
	{
		// Convert the host to lowercase to avoid ban evasion.
		std::string lowerhost(host.length(), '\0');
		std::transform(host.begin(), host.end(), lowerhost.begin(), ::tolower);

		std::string cloak;
		cloak.append(prefix).append(1, separator).append(Hash(lowerhost));

		const std::string visiblepart = Cloak::VisiblePart(host, hostparts, separator);
		if (!visiblepart.empty())
			cloak.append(1, separator).append(visiblepart);

		return cloak;
	}

	std::string Hash(const std::string& str)
	{
		std::string out;
		for (const auto chr : sha256->hmac(key, str).substr(0, segmentlen))
			out.push_back(table[chr & 0x1F]);
		return out;
	}

	std::string Wrap(const std::string& cloak, const char* separator)
	{
		std::string fullcloak;
		if (!prefix.empty())
			fullcloak.append(prefix).append(separator);
		fullcloak.append(cloak);
		if (!suffix.empty())
			fullcloak.append(separator).append(suffix);
		return fullcloak;
	}

public:
	SHA256Method(const Cloak::Engine* engine, const std::shared_ptr<ConfigTag>& tag, const std::string& k) ATTR_NOT_NULL(2)
		: Cloak::Method(engine)
		, hostparts(tag->getUInt("hostparts", 3, 1, UINT_MAX))
		, key(k)
		, prefix(tag->getString("prefix"))
		, sha256(engine->creator, "hash/sha256")
		, suffix(tag->getString("suffix", "ip"))
	{
		table = tag->getEnum("case", base32lower, {
			{ "upper", base32upper },
			{ "lower", base32lower }
		});
	}

	std::string Generate(LocalUser* user) override ATTR_NOT_NULL(2)
	{
		if (!sha256)
			return {};

		irc::sockets::sockaddrs sa(false);
		if (sa.from(user->GetRealHost()) && sa.addr() == user->client_sa.addr())
			return CloakAddress(user->client_sa);
		return CloakHost(user->GetRealHost(), '.');
	}

	std::string Generate(const std::string& hostip) override
	{
		if (!sha256)
			return {};

		irc::sockets::sockaddrs sa(false);
		return sa.from(hostip) ? CloakAddress(sa) : CloakHost(hostip, '.');
	}

	void GetLinkData(Module::LinkData& data, std::string& compatdata) override
	{
		// The value we use for cloaks when the sha2 module is missing.
		const std::string broken = "missing-sha2-module";

		// IMPORTANT: link data is sent over unauthenticated server links so we
		// can't directly send the key here. Instead we use dummy cloaks that
		// allow verification of or less the same thing.
		irc::sockets::sockaddrs sa;
		if (sa.from_ip("123.123.123.123"))
			data["cloak-v4"] = sha256 ? CloakAddress(sa) : broken;

		if (sa.from_ip("dead:beef:cafe::"))
			data["cloak-v6"] = sha256 ? CloakAddress(sa) : broken;

		data["cloak-host"] = sha256 ? CloakHost("cloak.inspircd.org",   '.') : broken;
		data["cloak-unix"] = sha256 ? CloakHost("/inspircd/cloak.sock", '/') : broken;
		data["host-parts"] = ConvToStr(hostparts);
		data["prefix"]     = prefix;
		data["suffix"]     = suffix;
	}
};

class SHA256Engine final
	: public Cloak::Engine
{
private:
	// The minimum length of a cloak key.
	static constexpr size_t minkeylen = 30;

public:
	SHA256Engine(Module* Creator)
		: Cloak::Engine(Creator, "hmac-sha256")
	{
	}

	Cloak::MethodPtr Create(const std::shared_ptr<ConfigTag>& tag, bool primary) override
	{
		// Ensure that we have the <cloak:key> parameter.
		const std::string key = tag->getString("key");
		if (key.length() < minkeylen)
			throw ModuleException(creator, "Your cloak key should be at least " + ConvToStr(minkeylen) + " characters long, at " + tag->source.str());

		return std::make_shared<SHA256Method>(this, tag, key);
	}
};

class ModuleCloakSHA256 final
	: public Module
{
private:
	SHA256Engine cloakengine;

public:
	ModuleCloakSHA256()
		: Module(VF_VENDOR, "Provides the hmac-sha256 cloak engine.")
		, cloakengine(this)
	{
	}
};

MODULE_INIT(ModuleCloakSHA256)
