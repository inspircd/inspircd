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


#include <sha1/sha1.c>

#include "inspircd.h"
#include "modules/hash.h"

#define SHA1_BLOCK_SIZE 64
#define SHA1_DIGEST_SIZE 20

class SHA1Context final
	: public Hash::Context
{
private:
	SHA1_CTX context;

public:
	SHA1Context()
	{
		SHA1Init(&context);
	}

	void Update(const unsigned char *data, size_t len) override
	{
		SHA1Update(&context, data, static_cast<uint32_t>(len));
	}

	std::string Finalize() override
	{
		std::vector<unsigned char> digest(SHA1_DIGEST_SIZE);
		SHA1Final(digest.data(), &context);
		return std::string(reinterpret_cast<const char *>(digest.data()), digest.size());
	}
};

class SHA1Provider final
	: public Hash::Provider
{
private:
	Hash::HMACProvider hmacsha1algo;

public:
	SHA1Provider(Module* mod, const std::string& algorithm)
		: Hash::Provider(mod, algorithm, SHA1_DIGEST_SIZE, SHA1_BLOCK_SIZE)
		, hmacsha1algo(mod, algorithm)
	{
	}

	std::unique_ptr<Hash::Context> CreateContext() override
	{
		return std::make_unique<SHA1Context>();
	}
};

class ModuleHashSHA1 final
	: public Module
{
private:
	SHA1Provider sha1algo;

public:
	ModuleHashSHA1()
		: Module(VF_VENDOR, "Allows other modules to generate SHA-1 hashes.")
		, sha1algo(this, "sha1")
	{
	}

	void init() override
	{
		sha1algo.Check({
			{ "da39a3ee5e6b4b0d3255bfef95601890afd80709", "" },
			{ "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12", "The quick brown fox jumps over the lazy dog" },
		});
	}
};

MODULE_INIT(ModuleHashSHA1)
