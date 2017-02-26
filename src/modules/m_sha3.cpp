/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 Peter Powell <petpow@saberuk.com>
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

/*
 * This module is based on the public domain Keccak reference implementation by
 * the Keccak, Keyak and Ketje Teams, namely, Guido Bertoni, Joan Daemen,
 * Michaël Peeters, Gilles Van Assche and Ronny Van Keer.
 *
 * Source: https://github.com/gvanas/KeccakCodePackage/
 */


#include "inspircd.h"
#include "modules/hash.h"

class SHA3Provider : public HashProvider
{
 private:
	int capacity;


 public:
	std::string GenerateRaw(const std::string& data) CXX11_OVERRIDE
	{
	}

	SHA3Provider(Module* Creator, const std::string& Name, unsigned int Rate, unsigned int Capacity, int OutputSize)
		: HashProvider(Creator, Name, OutputSize, Rate / 8)
		, capacity(Capacity)
	{
	}
};

#define SHA3_TEST(PROVIDER, NAME, EXPECTED) \
	do { \
		const std::string result = PROVIDER.Generate(""); \
		if (result != EXPECTED) { \
			throw ModuleException("CRITICAL: " NAME " implementation is producing incorrect results! Please report this as soon as possible."); \
		} \
	} while (0)

class ModuleSHA3 : public Module
{
	SHA3Provider sha3_224;
	SHA3Provider sha3_256;
	SHA3Provider sha3_384;
	SHA3Provider sha3_512;

 public:
	ModuleSHA3()
		: sha3_224(this, "sha3-224", 1152, 448,  28)
		, sha3_256(this, "sha3-256", 1088, 512,  32)
		, sha3_384(this, "sha3-384", 832,  768,  48)
		, sha3_512(this, "sha3-512", 576,  1024, 64)
	{
	}

	void init() CXX11_OVERRIDE
	{
		SHA3_TEST(sha3_224, "SHA3-224", "6b4e03423667dbb73b6e15454f0eb1abd4597f9a1b078e3f5b5a6bc7");
		SHA3_TEST(sha3_256, "SHA3-256", "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a");
		SHA3_TEST(sha3_384, "SHA3-384", "0c63a75b845e4f7d01107d852e4c2485c51a50aaaa94fc61995e71bbee983a2ac3713831264adb47fb6bd1e058d5f004");
		SHA3_TEST(sha3_512, "SHA3-512", "a69f73cca23a9ac5c8b567dc185a756e97c982164fe25859e0d1dcc1475c80a615b2123af1f5f94c11e3e9402c3ac558f500199d95b6d3e301758586281dcd26");
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		static std::string testhash = sha3_224.Generate("testhash");
		return Version("Implements support for the SHA-3 hash algorithm.", VF_VENDOR, testhash);
	}
};

MODULE_INIT(ModuleSHA3)
