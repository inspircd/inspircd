/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Daniel Vassdal <shutter@canternet.org>
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

/// $CompilerFlags: find_compiler_flags("libargon2" "")

/// $LinkerFlags: find_linker_flags("libargon2" "-llibargon2")

/// $PackageInfo: require_system("arch") argon2 pkgconf
/// $PackageInfo: require_system("darwin") argon2 pkg-config
/// $PackageInfo: require_system("debian" "9.0") libargon2-0 pkg-config
/// $PackageInfo: require_system("ubuntu" "18.04") libargon2-0-dev pkg-config


#include "inspircd.h"
#include "modules/hash.h"

#ifdef __GNUC__
# pragma GCC diagnostic push
#endif

// Fix warnings about the use of `long long` on C++03
#if defined __clang__
# pragma clang diagnostic ignored "-Wc++11-long-long"
#elif defined __GNUC__
# pragma GCC diagnostic ignored "-Wlong-long"
#endif

#include <argon2.h>

struct ProviderConfig
{
	uint32_t outlen;
	uint32_t version;
	uint32_t t_cost;
	uint32_t m_cost;
	uint32_t lanes;
	uint32_t threads;
	uint32_t salt_length;
};

class HashArgon2 : public HashProvider
{
	const Argon2_type argon2Type;
	ProviderConfig config;

	static void ReadConfig(ConfigTag* tag, ProviderConfig* config, ProviderConfig* def)
	{
		// 2^17 = 131072 = 128 MiB
		uint32_t def_m_cost = def != NULL ? def->m_cost : (1 << 17);
		uint32_t def_t_cost = def != NULL ? def->t_cost : 3;
		uint32_t def_lanes = def != NULL ? def->lanes : 1;
		uint32_t def_threads = def != NULL ? def->threads : 1;
		uint32_t def_outlen = def != NULL ? def->outlen : 32;
		uint32_t def_salt_length = def != NULL ? def->salt_length : 16;
		uint32_t def_version = def != NULL ? def->version : 13;

		config->m_cost = tag->getUInt("memory", def_m_cost, ARGON2_MIN_MEMORY, ARGON2_MAX_MEMORY);
		config->t_cost = tag->getUInt("iterations", def_t_cost, 1);
		config->lanes = tag->getUInt("lanes", def_lanes, ARGON2_MIN_LANES, ARGON2_MAX_LANES);
		config->threads = tag->getUInt("threads", def_threads, ARGON2_MIN_THREADS, ARGON2_MAX_THREADS);
		config->outlen = tag->getUInt("length", def_outlen, ARGON2_MIN_OUTLEN, ARGON2_MAX_OUTLEN);
		config->salt_length = tag->getUInt("saltlength", def_salt_length, ARGON2_MIN_SALT_LENGTH, ARGON2_MAX_SALT_LENGTH);
		config->version = SanitizeArgon2Version(tag->getUInt("version", def_version));
	}

	static uint32_t SanitizeArgon2Version(const int version)
	{
		// Note, 10 is 0x10, and 13 is 0x13. Refering to it as
		// dec 10 or 13 in the config file, for the name to
		// match better.
		if (version == 10) return ARGON2_VERSION_10;
		if (version == 13) return ARGON2_VERSION_13;

		ServerInstance->Logs->Log("MODULE", LOG_DEFAULT, "Unknown Argon2 version specified, assuming 13");
		return ARGON2_VERSION_13;
	}

 public:
	void ReadConfig()
	{
		ProviderConfig defaultConfig;
		ConfigTag* tag = ServerInstance->Config->ConfValue("argon2");
		ReadConfig(tag, &defaultConfig, NULL);

		std::string argonType = name.substr(name.find('/') + 1);;
		tag = ServerInstance->Config->ConfValue(argonType);
		ReadConfig(tag, &config, &defaultConfig);
	}

	bool Compare(const std::string& input, const std::string& hash) CXX11_OVERRIDE
	{
		int result = argon2_verify(
			hash.c_str(),
			input.c_str(),
			input.length(),
			argon2Type);

		return result == ARGON2_OK;
	}

	std::string GenerateRaw(const std::string& data) CXX11_OVERRIDE
	{
		std::string salt = ServerInstance->GenRandomStr(config.salt_length, false);

		size_t encodedLen = argon2_encodedlen(
			config.t_cost,
			config.m_cost,
			config.lanes,
			config.salt_length,
			config.outlen,
			argon2Type);

		std::vector<char> raw_data(config.outlen);
		std::vector<char> encoded_data(encodedLen + 1);

		int argonResult = argon2_hash(
			config.t_cost,
			config.m_cost,
			config.threads,
			data.c_str(),
			data.length(),
			salt.c_str(),
			salt.length(),
			&raw_data[0],
			raw_data.size(),
			&encoded_data[0],
			encoded_data.size(),
			argon2Type,
			config.version);

		if (argonResult != ARGON2_OK)
		{
			std::string errorMessage = std::string(argon2_error_message(argonResult));
			throw ModuleException("Argon2 hashing failed!: " + errorMessage);
		}

		// This isn't the raw version, but we don't have
		// the facilities to juggle around the extra state required
		// to do anything useful with them if we don't encode them.
		// So we pretend this is the raw version, and instead make
		// ToPrintable return its input.
		std::string result = std::string(&encoded_data[0]);
		return result;
	}

	std::string ToPrintable(const std::string& raw) CXX11_OVERRIDE
	{
		return raw;
	}

	HashArgon2(Module* parent, const std::string& hashName, Argon2_type type)
		: HashProvider(parent, hashName), argon2Type(type), config(ProviderConfig())
	{
	}
};

class ModuleArgon2 : public Module
{
	HashArgon2 argon2i;
	HashArgon2 argon2d;
	HashArgon2 argon2id;

 public:
	ModuleArgon2() :
		argon2i(this, "argon2i", Argon2_i),
		argon2d(this, "argon2d", Argon2_d),
		argon2id(this, "argon2id", Argon2_id)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Allows other modules to generate Argon2 hashes.", VF_VENDOR);
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		argon2i.ReadConfig();
		argon2d.ReadConfig();
		argon2id.ReadConfig();
	}
};

MODULE_INIT(ModuleArgon2)

#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif
