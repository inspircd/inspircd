/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2022-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2020 Elizabeth Myers <elizabeth@interlinked.me>
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

/// $CompilerFlags: find_compiler_flags("libargon2")
/// $LinkerFlags: find_linker_flags("libargon2")

/// $PackageInfo: require_system("arch") argon2 pkgconf
/// $PackageInfo: require_system("darwin") argon2 pkg-config
/// $PackageInfo: require_system("debian" "10.0") libargon2-dev pkg-config
/// $PackageInfo: require_system("ubuntu" "18.04") libargon2-0-dev pkg-config


#include "inspircd.h"
#include "modules/hash.h"

#include <argon2.h>

class ProviderConfig final
{
public:
	uint32_t iterations;
	uint32_t lanes;
	uint32_t memory;
	uint32_t outlen;
	uint32_t saltlen;
	uint32_t threads;
	Argon2_version version;

	ProviderConfig()
	{
		// Nothing interesting happens here.
	}

	ProviderConfig(const std::string& tagname, ProviderConfig* def)
	{
		const auto& tag = ServerInstance->Config->ConfValue(tagname);

		uint32_t def_iterations = def ? def->iterations : 3;
		this->iterations = tag->getNum<uint32_t>("iterations", def_iterations, 1);

		uint32_t def_lanes = def ? def->lanes : 1;
		this->lanes = tag->getNum<uint32_t>("lanes", def_lanes, ARGON2_MIN_LANES, ARGON2_MAX_LANES);

		uint32_t def_memory = def ? def->memory : 131072; // 128 MiB
		this->memory = tag->getNum<uint32_t>("memory", def_memory, ARGON2_MIN_MEMORY, ARGON2_MAX_MEMORY);

		uint32_t def_outlen = def ? def->outlen : 32;
		this->outlen = tag->getNum<int32_t>("length", def_outlen, ARGON2_MIN_OUTLEN, ARGON2_MAX_OUTLEN);

		uint32_t def_saltlen = def ? def->saltlen : 16;
		this->saltlen = tag->getNum<uint32_t>("saltlength", def_saltlen, ARGON2_MIN_SALT_LENGTH, ARGON2_MAX_SALT_LENGTH);

		uint32_t def_threads = def ? def->threads : 1;
		this->threads = tag->getNum<uint32_t>("threads", def_threads, ARGON2_MIN_THREADS, ARGON2_MAX_THREADS);

		Argon2_version def_version = def ? def->version : ARGON2_VERSION_13;
		this->version = tag->getEnum("version", def_version, {
			{ "10", ARGON2_VERSION_10 },
			{ "13", ARGON2_VERSION_13 },
		});
	}
};

class HashArgon2 final
	: public HashProvider
{
private:
	const Argon2_type argon2Type;

public:
	ProviderConfig config;

	bool Compare(const std::string& input, const std::string& hash) override
	{
		int result = argon2_verify(
			hash.c_str(),
			input.c_str(),
			input.length(),
			argon2Type);

		return result == ARGON2_OK;
	}

	std::string GenerateRaw(const std::string& data) override
	{
		const std::string salt = ServerInstance->GenRandomStr(config.saltlen, false);

		size_t encodedLen = argon2_encodedlen(
			config.iterations,
			config.memory,
			config.lanes,
			config.saltlen,
			config.outlen,
			argon2Type);

		std::vector<char> raw_data(config.outlen);
		std::vector<char> encoded_data(encodedLen + 1);

		int argonResult = argon2_hash(
			config.iterations,
			config.memory,
			config.threads,
			data.c_str(),
			data.length(),
			salt.c_str(),
			salt.length(),
			raw_data.data(),
			raw_data.size(),
			encoded_data.data(),
			encoded_data.size(),
			argon2Type,
			config.version);

		if (argonResult != ARGON2_OK)
			throw ModuleException(creator, "Argon2 hashing failed: " + std::string(argon2_error_message(argonResult)));

		// This isn't the raw version, but we don't have
		// the facilities to juggle around the extra state required
		// to do anything useful with them if we don't encode them.
		// So we pretend this is the raw version, and instead make
		// ToPrintable return its input.
		return std::string(encoded_data.data(), encoded_data.size());
	}

	std::string ToPrintable(const std::string& raw) override
	{
		return raw;
	}

	HashArgon2(Module* parent, const std::string& hashName, Argon2_type type)
		: HashProvider(parent, hashName)
		, argon2Type(type)

	{
	}
};

class ModuleArgon2 final
	: public Module
{
private:
	HashArgon2 argon2i;
	HashArgon2 argon2d;
	HashArgon2 argon2id;

public:
	ModuleArgon2()
		: Module(VF_VENDOR, "Allows other modules to generate Argon2 hashes.")
		, argon2i(this, "argon2i", Argon2_i)
		, argon2d(this, "argon2d", Argon2_d)
		, argon2id(this, "argon2id", Argon2_id)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		ProviderConfig defaultConfig("argon2", nullptr);
		argon2i.config = ProviderConfig("argon2i", &defaultConfig);
		argon2d.config = ProviderConfig("argon2d", &defaultConfig);
		argon2id.config = ProviderConfig("argon2id", &defaultConfig);
	}
};

MODULE_INIT(ModuleArgon2)
