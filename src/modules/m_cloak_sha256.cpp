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


/// $CompilerFlags: require_library("libpsl") find_compiler_flags("libpsl") -DHAS_LIBPSL
/// $LinkerFlags: require_library("libpsl") find_linker_flags("libpsl")

/// $PackageInfo: require_system("arch") libpsl pkgconf
/// $PackageInfo: require_system("darwin") libpsl pkg-config
/// $PackageInfo: require_system("debian") libpsl-dev pkg-config
/// $PackageInfo: require_system("ubuntu") libpsl-dev pkg-config

#ifdef _WIN32
# if __has_include(<libpsl.h>)
#  define HAS_LIBPSL
#  pragma comment(lib, "psl.lib")
# endif
#endif

#ifdef HAS_LIBPSL
# include <libpsl.h>
#else
typedef void psl_ctx_t;
#endif

#include "inspircd.h"
#include "modules/cloak.h"
#include "modules/hash.h"
#include "utility/string.h"

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

	// Whether to cloak the hostname if available.
	const bool cloakhost;

	// The number of parts of the hostname shown.
	const unsigned long hostparts;

	// The secret used for generating cloaks.
	const std::string key;

	// The number of parts of the UNIX socket path shown.
	const unsigned long pathparts;

	// The prefix for cloaks (e.g. MyNet).
	const std::string prefix;

#ifdef HAS_LIBPSL
	// Handle to the Public Suffix List library.
	psl_ctx_t* psl;
#endif

	// Dynamic reference to the sha256 implementation.
	dynamic_reference_nocheck<HashProvider> sha256;

	// The base32 table used when encoding.
	const unsigned char* table;

	// The suffix for IP cloaks (e.g. IP).
	const std::string suffix;

	std::string CloakAddress(const irc::sockets::sockaddrs& sa)
	{
		switch (sa.family())
		{
			case AF_INET:
				return CloakIPv4(sa.in4.sin_addr.s_addr);
			case AF_INET6:
				return CloakIPv6(sa.in6.sin6_addr.s6_addr);
			case AF_UNIX:
				return CloakHost(sa.un.sun_path, '/', pathparts);
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

		const std::string alpha = Hash(INSP_FORMAT("{}.{}.{}.{}", a, b, c, d));
		const std::string beta  = Hash(INSP_FORMAT("{}.{}.{}", a, b, c));
		const std::string gamma = Hash(INSP_FORMAT("{}.{}", a, b));

		return Wrap(INSP_FORMAT("{}.{}.{}", alpha, beta, gamma), suffix, '.');
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

		const std::string alpha = Hash(INSP_FORMAT("{:x}:{:x}:{:x}:{:x}:{:x}:{:x}:{:x}:{:x}", a, b, c, d, e, f, g, h));
		const std::string beta  = Hash(INSP_FORMAT("{:x}:{:x}:{:x}:{:x}:{:x}:{:x}:{:x}", a, b, c, d, e, f, g));
		const std::string gamma = Hash(INSP_FORMAT("{:x}:{:x}:{:x}:{:x}", a, b, c, d));

		return Wrap(INSP_FORMAT("{}:{}:{}", alpha, beta, gamma), suffix, ':');
	}

	std::string CloakHost(const std::string& host, char separator, unsigned long parts)
	{
		// Attempt to divine the public part of the hostname.
		std::string visiblepart;
#ifdef HAS_LIBPSL
		if (psl && separator == '.')
		{
			// Attempt to look up the suffix with libpsl.
			const char* publicsuffix = psl_registrable_domain(psl, host.c_str());
			if (!publicsuffix || publicsuffix == host)
				publicsuffix = psl_unregistrable_domain(psl, host.c_str());
			if (publicsuffix && publicsuffix != host)
				visiblepart = publicsuffix;
		}
#endif

		// If libpsl failed to find a suffix or wasn't available fall back.
		if (visiblepart.empty() && parts)
			visiblepart = Cloak::VisiblePart(host, parts, separator);

		// Convert the host to lowercase to avoid ban evasion.
		std::string lowerhost(host.length(), '\0');
		std::transform(host.begin(), host.end(), lowerhost.begin(), ::tolower);

		return Wrap(Hash(lowerhost), visiblepart, separator);
	}

	std::string Hash(const std::string& str)
	{
		std::string out;
		for (const auto chr : sha256->hmac(key, str).substr(0, segmentlen))
			out.push_back(table[chr & 0x1F]);
		return out;
	}

	std::string Wrap(const std::string& cloak, const std::string& cloaksuffix, char separator)
	{
		std::string fullcloak;
		if (!prefix.empty())
			fullcloak.append(prefix).append(1, separator);
		fullcloak.append(cloak);
		if (!cloaksuffix.empty())
			fullcloak.append(1, separator).append(cloaksuffix);
		return fullcloak;
	}

public:
	SHA256Method(const Cloak::Engine* engine, const std::shared_ptr<ConfigTag>& tag, const std::string& k, psl_ctx_t* p, bool ch) ATTR_NOT_NULL(2)
		: Cloak::Method(engine, tag)
		, cloakhost(ch)
		, hostparts(tag->getNum<unsigned long>("hostparts", 3, 0, ServerInstance->Config->Limits.MaxHost / 2))
		, key(k)
		, pathparts(tag->getNum<unsigned long>("pathparts", 1, 0, ServerInstance->Config->Limits.MaxHost / 2))
		, prefix(tag->getString("prefix"))
#ifdef HAS_LIBPSL
		, psl(p)
#endif
		, sha256(engine->creator, "hash/sha256")
		, suffix(tag->getString("suffix", "ip"))
	{
		table = tag->getEnum("case", base32lower, {
			{ "upper", base32upper },
			{ "lower", base32lower }
		});
	}

#ifdef HAS_PSL
	~SHA256Method() override
	{
		if (psl)
			psl_free(psl);
	}
#endif

	std::string Generate(LocalUser* user) override ATTR_NOT_NULL(2)
	{
		if (!sha256 || !MatchesUser(user))
			return {};

		irc::sockets::sockaddrs sa(false);
		if (!cloakhost || (sa.from(user->GetRealHost()) && sa.addr() == user->client_sa.addr()))
			return CloakAddress(user->client_sa);

		return CloakHost(user->GetRealHost(), '.', hostparts);
	}

	std::string Generate(const std::string& hostip) override
	{
		if (!sha256)
			return {};

		irc::sockets::sockaddrs sa(false);
		if (sa.from(hostip))
			return CloakAddress(sa);

		if (cloakhost)
			return CloakHost(hostip, '.', hostparts);

		return {}; // Only reachable on hmac-sha256-ip.
	}

	void GetLinkData(Module::LinkData& data, std::string& compatdata) override
	{
		// The value we use for cloaks when the sha2 module is missing.
		const std::string broken = "missing-sha2-module";

		// IMPORTANT: link data is sent over unauthenticated server links so we
		// can't directly send the key here. Instead we use dummy cloaks that
		// allow verification of or less the same thing.
		data["cloak-v4"]   = sha256 ? Generate("123.123.123.123")                        : broken;
		data["cloak-v6"]   = sha256 ? Generate("dead:beef:cafe::")                       : broken;
		data["cloak-host"] = sha256 ? Generate("extremely.long.inspircd.cloak.example")  : broken;
		data["cloak-unix"] = sha256 ? Generate("/extremely/long/inspircd/cloak.example") : broken;

		data["host-parts"] = ConvToStr(hostparts);
		data["path-parts"] = ConvToStr(pathparts);
		data["prefix"]     = prefix;
		data["suffix"]     = suffix;

#ifdef HAS_PSL
		data["using-psl"] = psl ? "yes" : "no";
#else
		data["using-psl"] = "no";
#endif
	}

	bool IsLinkSensitive() const override
	{
		// This method always wants to be at the front.
		return true;
	}
};

class SHA256Engine final
	: public Cloak::Engine
{
private:
	// The minimum length of a cloak key.
	static constexpr size_t minkeylen = 30;

	// Whether to cloak the hostname if available.
	const bool cloakhost;

	// Dynamic reference to the sha256 implementation.
	dynamic_reference_nocheck<HashProvider> sha256;

public:
	SHA256Engine(Module* Creator, const std::string& Name, bool ch)
		: Cloak::Engine(Creator, Name)
		, cloakhost(ch)
		, sha256(Creator, "hash/sha256")
	{
	}

	Cloak::MethodPtr Create(const std::shared_ptr<ConfigTag>& tag, bool primary) override
	{
		if (!sha256)
			throw ModuleException(creator, "Unable to create a " + name.substr(6) + " cloak without the sha2 module, at" + tag->source.str());

		// Ensure that we have the <cloak:key> parameter.
		const std::string key = tag->getString("key");
		if (key.length() < minkeylen)
			throw ModuleException(creator, "Your cloak key should be at least " + ConvToStr(minkeylen) + " characters long, at " + tag->source.str());

		psl_ctx_t* psl = nullptr;
		std::string psldb = tag->getString("psl");
		if (cloakhost && !psldb.empty())
		{
#ifdef HAS_LIBPSL
			if (insp::equalsci(psldb, "system"))
			{
				psldb = psl_dist_filename();
				if (psldb.empty())
					throw ModuleException(creator, "You specified \"system\" in <cloak:psl> but libpsl was built without a system copy, at " + tag->source.str());
			}

			psl = psl_load_file(psldb.c_str());
			if (!psl)
				throw ModuleException(creator, "The database specified in <cloak:psl> (" + psldb + ") does not exist, at " + tag->source.str());
#else
			throw ModuleException(creator, "You specified <cloak:psl> but InspIRCd was built without libpsl, at " + tag->source.str());
#endif
		}

		return std::make_shared<SHA256Method>(this, tag, key, psl, cloakhost);
	}
};

class ModuleCloakSHA256 final
	: public Module
{
private:
	SHA256Engine addrcloak;
	SHA256Engine hostcloak;

public:
	ModuleCloakSHA256()
		: Module(VF_VENDOR, "Adds the hmac-sha256 and hmac-sha256-ip cloaking methods for use with the cloak module.")
		, addrcloak(this, "hmac-sha256-addr", false)
		, hostcloak(this, "hmac-sha256", true)
	{
	}

#ifdef HAS_LIBPSL
	void init() override
	{
		ServerInstance->Logs.Normal(MODNAME, "Module was compiled against libpsl version {} and is running against version {}",
			PSL_VERSION, psl_get_version());
	}
#endif
};

MODULE_INIT(ModuleCloakSHA256)
