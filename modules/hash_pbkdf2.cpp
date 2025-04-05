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


#include "inspircd.h"
#include "modules/hash.h"

namespace
{
	// NOTE: We use an extremely low iteration count in the checks to
	// avoid blocking for a long time when starting up.
	const std::map<std::string, std::map<std::string, std::string>> checks = {
		{ "sha1", {
			{ "100:bsOVI55xIWFZxO7Ox4lb8Q:B9baArs7lJR9kFVLFAbpEA", "The quick brown fox jumps over the lazy dog" },
		}},
		{ "sha224", {
			{ "100:7mCEabsDkJmv7yphDJ00Zg:ZpVPEq7DshpaCwPpE6WiTA", "The quick brown fox jumps over the lazy dog" },
		}},
		{ "sha256", {
			{ "100:eV0OGnSmBtvC1Y7pUc5Odg:I4yDKQWppE2rrHJDX01V2g", "The quick brown fox jumps over the lazy dog" },
		}},
		{ "sha384", {
			{ "100:uj0W7gdL6BlHRst3jJwaFA:8FtaHvqYgZwsSLc2a2Fiig", "The quick brown fox jumps over the lazy dog" },
		}},
		{ "sha512", {
			{ "100:OOjcsv69e87ItbWU6+TX0w:5VoQbRnulaCeJlbA44UMFw", "The quick brown fox jumps over the lazy dog" },
		}},
	};

	std::string PBKDF2(Hash::Provider* provider, const std::string& plain, const std::string& salt, uint32_t iterations, size_t length)
	{
		if (!provider || provider->IsKDF())
			return {};

		std::string output;
		auto blocks = static_cast<size_t>(std::ceil(static_cast<double>(length) / provider->digest_size));
		for (size_t block = 1; block <= blocks; ++block)
		{
			auto block_data = salt;
			for (size_t i = 0; i < 4; ++i)
				block_data.push_back(block >> (24 - i * 8) & 0x0F);

			auto block_hash = Hash::HMAC(provider, plain, block_data);
			auto prevhash = block_hash;
			for (size_t iteration = 1; iteration < iterations; ++iteration)
			{
				auto hash = Hash::HMAC(provider, plain, prevhash);
				for (size_t i = 0; i < provider->digest_size; ++i)
					block_hash[i] ^= hash[i];
				prevhash = hash;
			}
			output += block_hash;
		}

		output.erase(length);
		return output;
	}
}

struct PBKDF2Config final
{
	uint32_t iterations = 0;
	size_t key_length = 0;
	size_t salt_length = 0;

	void ReadConfig(const std::shared_ptr<ConfigTag>& tag, PBKDF2Config* def = nullptr)
	{
		iterations = tag->getNum<uint32_t>("iterations", def ? def->iterations : 15'000, 1000);
		key_length = tag->getNum<size_t>("length", def ? def->key_length : 32, 1, 1024);
		salt_length = tag->getNum<size_t>("saltlength", def ? def->salt_length : key_length, 1, 1024);
	}
};

class PBKDF2Context final
	: public Hash::Context
{
private:
	std::string buffer;
	const PBKDF2Config& config;
	Hash::ProviderRef provider;

	std::string GenerateSalt()
	{
		std::vector<char> salt(config.salt_length);
		ServerInstance->GenRandom(salt.data(), salt.size());
		return std::string(salt.data(), salt.size());
	}

public:
	PBKDF2Context(const PBKDF2Config& c, const Hash::ProviderRef& prov)
		: config(c)
		, provider(prov)
	{
	}

	void Update(const unsigned char *data, size_t len) override
	{
		buffer.append(reinterpret_cast<const char *>(data), len);
	}

	std::string Finalize() override
	{
		if (!provider)
			return {}; // No underlying hash (should never happen).

		auto salt = this->GenerateSalt();
		auto hash = PBKDF2(*provider, buffer, salt, config.iterations, config.key_length);
		if (hash.empty())
			return {};

		this->buffer.clear();
		return FMT::format("{}:{}:{}", config.iterations, Base64::Encode(hash), Base64::Encode(salt));
	}
};

class PBKDF2Provider final
	: public Hash::Provider
{
private:
	Hash::ProviderRef provider;

public:
	PBKDF2Config config;

	PBKDF2Provider(Module* mod, const std::string& algorithm)
		: Hash::Provider(mod, FMT::format("pbkdf2-hmac-{}", algorithm))
		, provider(mod, algorithm)
	{
	}

	bool Compare(const std::string& hash, const std::string& plain) override
	{
		if (!provider)
			return false; // No underlying hash (should never happen).

		uint32_t iterations;
		std::string key, salt;
		irc::sepstream stream(hash, ':');
		if (!stream.GetNumericToken(iterations) || !iterations || !stream.GetToken(key) || !stream.GetToken(salt))
			return false; // Malformed hash.

		auto rawkey = Base64::Decode(key);
		auto rawsalt = Base64::Decode(salt);

		auto expected = PBKDF2(*provider, plain, rawsalt, iterations, rawkey.length());
		return !expected.empty() && InspIRCd::TimingSafeCompare(rawkey, expected);
	}

	std::unique_ptr<Hash::Context> CreateContext() override
	{
		return std::make_unique<PBKDF2Context>(config, provider);
	}

	std::string ToPrintable(const std::string &hash) override
	{
		// We have no way to make this printable without the creating context
		// so we always return the printed form.
		return hash;
	}
};

class ModuleHashPBKDF2 final
	: public Module
{
private:
	insp::flat_map<std::string, PBKDF2Provider*> algos;
	insp::flat_map<std::string, PBKDF2Config> configs;
	PBKDF2Config defaultconfig;

	void Configure(PBKDF2Provider* algo)
	{
		auto it = configs.find(algo->GetAlgorithm());
		if (it == configs.end())
			algo->config = defaultconfig;
		else
			algo->config = it->second;
	}

public:
	ModuleHashPBKDF2()
		: Module(VF_VENDOR, "Allows other modules to generate PBKDF2 hashes.")
	{
	}

	~ModuleHashPBKDF2() override
	{
		for (const auto &[_, algo] : algos)
			delete algo;
	}

	void init() override
	{
		// Let ourself know about any existing services.
		for (const auto& [_, service] : ServerInstance->Modules.DataProviders)
			OnServiceAdd(*service);
	}


	void ReadConfig(ConfigStatus& status) override
	{
		// Read the config.
		PBKDF2Config newdefaultconfig;
		newdefaultconfig.ReadConfig(ServerInstance->Config->ConfValue("pbkdf2"));

		insp::flat_map<std::string, PBKDF2Config> newconfigs;
		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("pbkdf2prov"))
		{
			const auto hash = tag->getString("hash");
			if (hash.empty())
				throw ModuleException(this, "<pbkdf2prov:name> must not be empty, at {}", tag->source.str());

			PBKDF2Config newprovconfig;
			newprovconfig.ReadConfig(tag, &newdefaultconfig);
			if (!newconfigs.emplace(hash, newprovconfig).second)
				throw ModuleException(this, "<pbkdf2prov:name> must be unique, at {}", tag->source.str());
		}

		// Apply the config.
		std::swap(defaultconfig, newdefaultconfig);
		std::swap(configs, newconfigs);

		// Apply the config to the providers.
		for (const auto &[_, algo] : algos)
			Configure(algo);
	}

	void OnServiceAdd(ServiceProvider& service) override
	{
		if (!service.name.starts_with("hash/"))
			return; //  Not a hash provider.

		auto* hp = static_cast<Hash::Provider*>(&service);
		if (hp->IsKDF())
			return; // Can't use PBKDF2 with a KDF.

		auto* algo = new PBKDF2Provider(this, hp->GetAlgorithm());
		Configure(algo);

		try
		{
			auto check = checks.find(hp->GetAlgorithm());
			if (check != checks.end())
			{
				algo->Check(check->second);
			}
			else
			{
				ServerInstance->Logs.Debug("HASH", "The {} algorithm lacks runtime checks, unable to verify integrity.",
					hp->GetAlgorithm());
			}
		}
		catch (const ModuleException& err)
		{
			ServerInstance->Logs.Critical("HASH", "{}", err.GetReason());
			delete algo;
			return; // Broken algorithm.
		}

		algos.emplace(hp->GetAlgorithm(), algo);
		ServerInstance->Modules.AddService(*algo);
	}

	void OnServiceDel(ServiceProvider& service) override
	{
		if (!service.name.starts_with("hash/"))
			return; //  Not a hash provider.

		auto it = algos.find(static_cast<Hash::Provider&>(service).GetAlgorithm());
		if (it != algos.end())
		{
			delete it->second;
			algos.erase(it);
		}
	}

};

MODULE_INIT(ModuleHashPBKDF2)
