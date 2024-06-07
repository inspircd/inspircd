/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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


// Fix a collision between the Haiku uint64 typedef and the
// one from the sha2 library.
#ifdef __HAIKU__
# define uint64 sha2_uint64
#endif

#include <sha2/sha2.c>

#ifdef __HAIKU__
# undef uint64
#endif

#include "inspircd.h"
#include "modules/hash.h"

template<void (*SHA)(const unsigned char*, unsigned int, unsigned char*)>
class HashSHA2 final
	: public HashProvider
{
public:
	HashSHA2(Module* parent, const std::string& Name, unsigned int osize, unsigned int bsize)
		: HashProvider(parent, Name, osize, bsize)
	{
	}

	std::string GenerateRaw(const std::string& data) override
	{
		std::vector<char> bytes(out_size);
		SHA(reinterpret_cast<const unsigned char*>(data.data()), static_cast<unsigned int>(data.size()), reinterpret_cast<unsigned char*>(bytes.data()));
		return std::string(bytes.data(), bytes.size());
	}
};

class ModuleSHA2 final
	: public Module
{
private:
	HashSHA2<sha224> sha224algo;
	HashSHA2<sha256> sha256algo;
	HashSHA2<sha384> sha384algo;
	HashSHA2<sha512> sha512algo;

public:
	ModuleSHA2()
		: Module(VF_VENDOR, "Allows other modules to generate SHA-2 hashes.")
		, sha224algo(this, "sha224", SHA224_DIGEST_SIZE, SHA224_BLOCK_SIZE)
		, sha256algo(this, "sha256", SHA256_DIGEST_SIZE, SHA256_BLOCK_SIZE)
		, sha384algo(this, "sha384", SHA384_DIGEST_SIZE, SHA384_BLOCK_SIZE)
		, sha512algo(this, "sha512", SHA512_DIGEST_SIZE, SHA512_BLOCK_SIZE)
	{
	}
};

MODULE_INIT(ModuleSHA2)
