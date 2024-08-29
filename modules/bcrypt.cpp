/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2018-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014 Daniel Vassdal <shutter@canternet.org>
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

class BCryptProvider final
	: public HashProvider
{
private:
	std::string Salt()
	{
		char entropy[16];
		ServerInstance->GenRandom(entropy, std::size(entropy));

		char salt[32];
		if (!_crypt_gensalt_blowfish_rn("$2a$", rounds, entropy, sizeof(entropy), salt, sizeof(salt)))
			throw ModuleException(creator, "Could not generate salt - this should never happen");

		return salt;
	}

public:
	unsigned long rounds = 10;

	static std::string Generate(const std::string& data, const std::string& salt)
	{
		char hash[64];
		_crypt_blowfish_rn(data.c_str(), salt.c_str(), hash, sizeof(hash));
		return hash;
	}

	std::string GenerateRaw(const std::string& data) override
	{
		return Generate(data, Salt());
	}

	bool Compare(const std::string& input, const std::string& hash) override
	{
		return InspIRCd::TimingSafeCompare(Generate(input, hash), hash);
	}

	std::string ToPrintable(const std::string& raw) override
	{
		return raw;
	}

	BCryptProvider(Module* parent)
		: HashProvider(parent, "bcrypt", 60)
	{
	}
};

class ModuleBCrypt final
	: public Module
{
private:
	BCryptProvider bcrypt;

public:
	ModuleBCrypt()
		: Module(VF_VENDOR, "Allows other modules to generate bcrypt hashes.")
		, bcrypt(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& conf = ServerInstance->Config->ConfValue("bcrypt");
		bcrypt.rounds = conf->getNum<unsigned long>("rounds", 10, 1);
	}
};

MODULE_INIT(ModuleBCrypt)
