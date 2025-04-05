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

#include <bcrypt/crypt_blowfish.c>

class BCryptContext final
	: public Hash::Context
{
private:
	std::string buffer;

	std::string GenerateSalt()
	{
		char entropy[16];
		ServerInstance->GenRandom(entropy, std::size(entropy));

		char salt[32];
		if (!_crypt_gensalt_blowfish_rn("$2a$", rounds, entropy, sizeof(entropy), salt, sizeof(salt)))
		{
			ServerInstance->Logs.Debug("HASH", "Unable to generate a bcrypt salt: {}", strerror(errno));
			return {};
		}
		return salt;
	}

public:
	static unsigned long rounds;

	static std::string Hash(const std::string& data, const std::string& salt)
	{
		char hash[64];
		if (!_crypt_blowfish_rn(data.c_str(), salt.c_str(), hash, sizeof(hash)))
		{
			ServerInstance->Logs.Debug("HASH", "Unable to generate a bcrypt hash: {}", strerror(errno));
			return {};
		}
		return hash;
	}

	void Update(const unsigned char* data, size_t len) override
	{
		buffer.append(reinterpret_cast<const char *>(data), len);
	}

	std::string Finalize() override
	{
		auto salt = GenerateSalt();
		if (salt.empty())
			return {};
		return Hash(this->buffer, salt);
	}
};

unsigned long BCryptContext::rounds = 10;

class BCryptProvider final
	: public Hash::Provider
{
public:
	BCryptProvider(Module* mod)
		: Hash::Provider(mod, "bcrypt", 60)
	{
	}

	bool Compare(const std::string& hash, const std::string& plain) override
	{
		auto newhash = BCryptContext::Hash(plain, hash);
		return !newhash.empty() && InspIRCd::TimingSafeCompare(hash, newhash);
	}

	std::unique_ptr<Hash::Context> CreateContext() override
	{
		return std::make_unique<BCryptContext>();
	}

	std::string ToPrintable(const std::string& hash) override
	{
		// The crypt_blowfish library does not expose a raw form.
		return hash;
	}
};

class ModuleHashBCrypt final
	: public Module
{
private:
	BCryptProvider bcryptalgo;

public:
	ModuleHashBCrypt()
		: Module(VF_VENDOR, "Allows other modules to generate bcrypt hashes.")
		, bcryptalgo(this)
	{
	}

	void init() override
	{
		bcryptalgo.Check({
			{ "$2a$10$c9lUAuJmTYXEfNuLOiyIp.lZTMM.Rw5qsSAyZhvGT9EC3JevkUuOu", "" },
			{ "$2a$10$YV4jDSGs0ZtQbpL6IHtNO.lt5Q.uzghIohCcnERQVBGyw7QJMfyhe", "The quick brown fox jumps over the lazy dog" },
		});
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& conf = ServerInstance->Config->ConfValue("bcrypt");
		BCryptContext::rounds = conf->getNum<unsigned long>("rounds", 10, 1);
	}
};

MODULE_INIT(ModuleHashBCrypt)
