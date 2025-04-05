/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2025 Sadie Powell <sadie@witchery.services>
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

/// $PackageInfo: require_system("alpine") argon2-dev pkgconf
/// $PackageInfo: require_system("arch") argon2 pkgconf
/// $PackageInfo: require_system("darwin") argon2 pkg-config
/// $PackageInfo: require_system("debian~") libargon2-dev pkg-config

#include <argon2.h>

#include "inspircd.h"
#include "modules/hash.h"

struct Argon2Config final
{
	uint32_t memory_cost;
	uint32_t time_cost;
	uint32_t parallelism;
	uint32_t hash_length;
	uint32_t salt_length;
	const argon2_type type;

	Argon2Config(argon2_type at)
		: type(at)
	{
	}

	void ReadConfig(const std::string& tagname)
	{
		const auto& tag = ServerInstance->Config->ConfValue(tagname);
		const auto& ptag = ServerInstance->Config->ConfValue("argon2");

		auto get_value = [&tag, &ptag](const std::string& key, uint32_t def, uint32_t min, uint32_t max) {
			return tag->getNum<uint32_t>(key, ptag->getNum<uint32_t>(key, def, min, max), min, max);
		};

		memory_cost = get_value("memorycost", 131'072, ARGON2_MIN_MEMORY, ARGON2_MAX_MEMORY);
		time_cost   = get_value("timecost", 3, 1, 1000);
		parallelism = get_value("parallelism", 1, ARGON2_MIN_THREADS, ARGON2_MAX_THREADS);
		hash_length = get_value("length", 32, ARGON2_MIN_OUTLEN, ARGON2_MAX_OUTLEN);
		salt_length = get_value("saltlength", hash_length, ARGON2_MIN_SALT_LENGTH, ARGON2_MAX_SALT_LENGTH);
	}
};

class Argon2Context final
	: public Hash::Context
{
private:
	std::string buffer;
	const Argon2Config& config;

	std::string GenerateSalt()
	{
		std::vector<char> salt(config.salt_length);
		ServerInstance->GenRandom(salt.data(), salt.size());
		return std::string(salt.data(), salt.size());
	}

public:
	Argon2Context(const Argon2Config& c)
		: config(c)
	{
	}

	void Update(const unsigned char *data, size_t len) override
	{
		buffer.append(reinterpret_cast<const char *>(data), len);
	}

	std::string Finalize() override
	{
		auto salt = GenerateSalt();

		// Calculate the size of and allocate the output buffer.
		auto length = argon2_encodedlen(config.time_cost, config.memory_cost, config.parallelism,
			config.salt_length, config.hash_length, config.type);

		std::vector<char> digest(length);
		auto result = argon2_hash(config.time_cost, config.memory_cost, config.parallelism,
			buffer.c_str(), buffer.length(), salt.c_str(), salt.length(), nullptr,
			config.hash_length, digest.data(), digest.size(), config.type,
			ARGON2_VERSION_NUMBER);

		if (result == ARGON2_OK)
			return std::string(digest.data(), digest.size());

		ServerInstance->Logs.Debug("HASH", "Unable to generate an Argon2 hash: {}", argon2_error_message(result));
		return {};
	}
};

class Argon2Provider final
	: public Hash::Provider
{
public:
	Argon2Config config;

	Argon2Provider(Module *mod, argon2_type at)
		: Hash::Provider(mod, argon2_type2string(at, 0))
		, config(at)
	{
		config.ReadConfig(GetAlgorithm());
	}

	bool Compare(const std::string &hash, const std::string &plain) override
	{
		return argon2_verify(hash.c_str(), plain.c_str(), plain.length(), config.type) == ARGON2_OK;
	}

	std::unique_ptr<Hash::Context> CreateContext() override
	{
		return std::make_unique<Argon2Context>(this->config);
	}

	std::string ToPrintable(const std::string &hash) override
	{
		// We have no way to make this printable without the creating context
		// so we always return the printed form.
		return hash;
	}
};

class ModuleArgon2 final
	: public Module
{
private:
	Argon2Provider argon2dalgo;
	Argon2Provider argon2ialgo;
	Argon2Provider argon2idalgo;

public:
	ModuleArgon2()
		: Module(VF_VENDOR, "Allows other modules to generate Argon2 hashes.")
		, argon2dalgo(this, Argon2_d)
		, argon2ialgo(this, Argon2_i)
		, argon2idalgo(this, Argon2_id)
	{
	}

	void init() override
	{
		argon2dalgo.Check({
			{ "$argon2d$v=19$m=10,t=10,p=1$VGhlIHF1aWNrIGJyb3duIGZveCBqdW1wcyBvdmVyIHRoZSBsYXp5IGRvZw$fNS8JrvE8EqKwQ", "" },
			{ "$argon2d$v=19$m=10,t=10,p=1$VGhlIHF1aWNrIGJyb3duIGZveCBqdW1wcyBvdmVyIHRoZSBsYXp5IGRvZw$hTvpprMF0TwszQ", "The quick brown fox jumps over the lazy dog" },
		});
		argon2ialgo.Check({
			{ "$argon2i$v=19$m=10,t=10,p=1$VGhlIHF1aWNrIGJyb3duIGZveCBqdW1wcyBvdmVyIHRoZSBsYXp5IGRvZw$neE6hYxRp4TCJA", "" },
			{ "$argon2i$v=19$m=10,t=10,p=1$VGhlIHF1aWNrIGJyb3duIGZveCBqdW1wcyBvdmVyIHRoZSBsYXp5IGRvZw$/JAt4FdP1MFD+A", "The quick brown fox jumps over the lazy dog" },
		});
		argon2idalgo.Check({
			{ "$argon2id$v=19$m=10,t=10,p=1$VGhlIHF1aWNrIGJyb3duIGZveCBqdW1wcyBvdmVyIHRoZSBsYXp5IGRvZw$wuNeHixFDS6Tkg", "" },
			{ "$argon2id$v=19$m=10,t=10,p=1$VGhlIHF1aWNrIGJyb3duIGZveCBqdW1wcyBvdmVyIHRoZSBsYXp5IGRvZw$Po8RcmxZ7vHmdg", "The quick brown fox jumps over the lazy dog" },
		});
	}

	void ReadConfig(ConfigStatus& status) override
	{
		if (status.initial)
			return; // Already read.

		argon2ialgo.config.ReadConfig(argon2ialgo.GetAlgorithm());
		argon2dalgo.config.ReadConfig(argon2dalgo.GetAlgorithm());
		argon2idalgo.config.ReadConfig(argon2idalgo.GetAlgorithm());
	}
};

MODULE_INIT(ModuleArgon2)
