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

template <typename SHAContext,
	void (* SHAInit)(SHAContext *),
	void (* SHAUpdate)(SHAContext *, const unsigned char *, unsigned int),
	void (* SHAFinal)(SHAContext *, unsigned char *)>
class SHA2Context final
	: public Hash::Context
{
private:
	SHAContext context;
	const size_t digest_size;

public:
	SHA2Context(size_t ds)
		: digest_size(ds)
	{
		SHAInit(&context);
	}

	void Update(const unsigned char *data, size_t len) override
	{
		SHAUpdate(&context, data, static_cast<unsigned int>(len));
	}

	std::string Finalize() override
	{
		std::vector<unsigned char> digest(digest_size);
		SHAFinal(&context, digest.data());
		return std::string(reinterpret_cast<const char *>(digest.data()), digest.size());
	}
};

template <typename SHAContext,
	void (* SHAInit)(SHAContext *),
	void (* SHAUpdate)(SHAContext *, const unsigned char *, unsigned int),
	void (* SHAFinal)(SHAContext *, unsigned char *)>
class SHA2Provider final
	: public Hash::Provider
{
private:
	Hash::HMACProvider hmacsha2algo;

public:
	SHA2Provider(Module* mod, const std::string& algorithm, size_t ds, size_t bs)
		: Hash::Provider(mod, algorithm, ds, bs)
		, hmacsha2algo(mod, algorithm)
	{
	}

	bool IsPasswordSafe() const override
	{
		// Plain SHA-2 is not safe for password use as it can be decoded via the
		// use of a rainbow table. You should use HMAC-SHA-2 instead as it is not
		// vulnerable to this attack.
		return false;
	}

	std::unique_ptr<Hash::Context> CreateContext() override
	{
		return std::make_unique<SHA2Context<SHAContext, SHAInit, SHAUpdate, SHAFinal>>(this->digest_size);
	}
};

class ModuleHashSHA2 final
	: public Module
{
private:
	SHA2Provider<sha224_ctx, sha224_init, sha224_update, sha224_final> sha224algo;
	SHA2Provider<sha256_ctx, sha256_init, sha256_update, sha256_final> sha256algo;
	SHA2Provider<sha384_ctx, sha384_init, sha384_update, sha384_final> sha384algo;
	SHA2Provider<sha512_ctx, sha512_init, sha512_update, sha512_final> sha512algo;

public:
	ModuleHashSHA2()
		: Module(VF_VENDOR, "Allows other modules to generate SHA-2 hashes.")
		, sha224algo(this, "sha224", SHA224_DIGEST_SIZE, SHA224_BLOCK_SIZE)
		, sha256algo(this, "sha256", SHA256_DIGEST_SIZE, SHA256_BLOCK_SIZE)
		, sha384algo(this, "sha384", SHA384_DIGEST_SIZE, SHA384_BLOCK_SIZE)
		, sha512algo(this, "sha512", SHA512_DIGEST_SIZE, SHA512_BLOCK_SIZE)
	{
	}

	void init() override
	{
		sha224algo.Check({
			{ "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f", "" },
			{ "730e109bd7a8a32b1cb9d9a09aa2325d2430587ddbc0c38bad911525", "The quick brown fox jumps over the lazy dog" },
		});
		sha256algo.Check({
			{ "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "" },
			{ "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592", "The quick brown fox jumps over the lazy dog" },
		});
		sha384algo.Check({
			{ "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b", "" },
			{ "ca737f1014a48f4c0b6dd43cb177b0afd9e5169367544c494011e3317dbf9a509cb1e5dc1e85a941bbee3d7f2afbc9b1", "The quick brown fox jumps over the lazy dog" },
		});
		sha512algo.Check({
			{ "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e", "" },
			{ "07e547d9586f6a73f73fbac0435ed76951218fb7d0c8d788a309d785436bbb642e93a252a954f23912547d1e8a3b5ed6e1bfd7097821233fa0538f3db854fee6", "The quick brown fox jumps over the lazy dog" },
		});
	}
};

MODULE_INIT(ModuleHashSHA2)
