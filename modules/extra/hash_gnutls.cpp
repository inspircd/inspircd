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


/// $CompilerFlags: find_compiler_flags("gnutls")
/// $LinkerFlags: find_linker_flags("gnutls")

/// $PackageInfo: require_system("arch") gnutls pkgconf
/// $PackageInfo: require_system("darwin") gnutls pkg-config
/// $PackageInfo: require_system("debian~") libgnutls28-dev pkg-config
/// $PackageInfo: require_system("rhel~") gnutls-devel pkgconfig


#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>

#include "inspircd.h"
#include "modules/hash.h"

namespace
{
	template<typename Enum>
	std::string GetAlgoName(Enum algo)
	{
		std::string name = gnutls_mac_get_name(static_cast<gnutls_mac_algorithm_t>(algo));
		std::transform(name.begin(), name.end(), name.begin(), ::tolower);
		return name;
	}
}


class GnuTLSHMACContext final
	: public Hash::Context
{
private:
	gnutls_mac_algorithm_t algorithm;
	gnutls_hmac_hd_t context;
	std::vector<char> salt;

public:
	GnuTLSHMACContext(gnutls_mac_algorithm_t algo)
		: algorithm(algo)
		, salt(gnutls_hmac_get_len(algo))
	{
		ServerInstance->GenRandom(salt.data(), salt.size());
		gnutls_hmac_init(&context, algorithm, salt.data(), salt.size());
	}

	~GnuTLSHMACContext()
	{
		gnutls_hmac_deinit(context, nullptr);
	}


	void Update(const unsigned char *data, size_t len) override
	{
		gnutls_hmac(context, data, len);
	}

	std::string Finalize() override
	{
		std::vector<unsigned char> digest(gnutls_hmac_get_len(algorithm));
		gnutls_hmac_output(context, digest.data());
		return FMT::format("{}${}", Base64::Encode(salt.data(), salt.size()), Base64::Encode(digest.data(), digest.size()));
	}
};


class GnuTLSHMACProvider final
	: public Hash::Provider
{
private:
	gnutls_mac_algorithm_t algorithm;

public:
	GnuTLSHMACProvider(Module* mod, gnutls_mac_algorithm_t algo)
		: Hash::Provider(mod, FMT::format("hmac-{}", GetAlgoName(algo)))
		, algorithm(algo)
	{
	}

	bool Compare(const std::string& hash, const std::string& plain) override
	{
		auto sep = hash.find('$');
		if (sep == std::string::npos)
			return false; // Malformed hash.

		auto rawkey = Base64::Decode(hash.substr(0, sep));
		auto rawhash = Base64::Decode(hash.substr(sep + 1));

		std::vector<unsigned char> expectedbuf(gnutls_hmac_get_len(algorithm));
		if (gnutls_hmac_fast(algorithm, rawkey.data(), rawkey.length(), plain.data(), plain.length(), expectedbuf.data()) < 0)
			return false; // Hashing error.

		const std::string expected(reinterpret_cast<const char *>(expectedbuf.data()), expectedbuf.size());
		return InspIRCd::TimingSafeCompare(rawhash, expected);
	}

	std::unique_ptr<Hash::Context> CreateContext() override
	{
		return std::make_unique<GnuTLSHMACContext>(algorithm);
	}

	std::string ToPrintable(const std::string &hash) override
	{
		// We have no way to make this printable without the creating context
		// so we always return the printed form.
		return hash;
	}
};

class GnuTLSContext final
	: public Hash::Context
{
private:
	gnutls_digest_algorithm_t algorithm;
	gnutls_hash_hd_t context;

public:
	GnuTLSContext(gnutls_digest_algorithm_t algo)
		: algorithm(algo)
	{
		gnutls_hash_init(&context, algorithm);
	}

	~GnuTLSContext()
	{
		gnutls_hash_deinit(context, nullptr);
	}

	void Update(const unsigned char *data, size_t len) override
	{
		gnutls_hash(context, data, len);
	}

	std::string Finalize() override
	{
		std::vector<unsigned char> digest(gnutls_hash_get_len(algorithm));
		gnutls_hash_output(context, digest.data());
		return std::string(reinterpret_cast<const char *>(digest.data()), digest.size());
	}
};

class GnuTLSProvider final
	: public Hash::Provider
{
private:
	gnutls_digest_algorithm_t algorithm;
	GnuTLSHMACProvider hmacalgo;

public:
	GnuTLSProvider(Module* mod, gnutls_digest_algorithm_t algo, size_t bs)
		: Hash::Provider(mod, GetAlgoName(algo), gnutls_hash_get_len(algo), bs)
		, algorithm(algo)
		, hmacalgo(mod, static_cast<gnutls_mac_algorithm_t>(algo))
	{
	}

	std::unique_ptr<Hash::Context> CreateContext() override
	{
		return std::make_unique<GnuTLSContext>(algorithm);
	}

	bool IsPasswordSafe() const override
	{
		// Plain hashes are not safe for password use as they can be decoded via
		// the use of a rainbow table. You should use a HMAC hash instead as
		// they are not vulnerable to this attack.
		return false;
	}
};

class ModuleHashGnuTLS final
	: public Module
{
private:
	GnuTLSProvider sha1algo;
	GnuTLSProvider sha224algo;
	GnuTLSProvider sha256algo;
	GnuTLSProvider sha384algo;
	GnuTLSProvider sha512algo;
	GnuTLSProvider sha3224algo;
	GnuTLSProvider sha3256algo;
	GnuTLSProvider sha3384algo;
	GnuTLSProvider sha3512algo;

public:
	ModuleHashGnuTLS()
		: Module(VF_VENDOR, "Allows other modules to generate hashes using GnuTLS.")
		, sha1algo(this, GNUTLS_DIG_SHA1, 64)
		, sha224algo(this, GNUTLS_DIG_SHA224, 64)
		, sha256algo(this, GNUTLS_DIG_SHA256, 64)
		, sha384algo(this, GNUTLS_DIG_SHA384, 128)
		, sha512algo(this, GNUTLS_DIG_SHA512, 128)
		, sha3224algo(this, GNUTLS_DIG_SHA3_224, 144)
		, sha3256algo(this, GNUTLS_DIG_SHA3_256, 136)
		, sha3384algo(this, GNUTLS_DIG_SHA3_384, 104)
		, sha3512algo(this, GNUTLS_DIG_SHA3_512, 72)
	{
		gnutls_global_init();
	}

	~ModuleHashGnuTLS()
	{
		gnutls_global_deinit();
	}

	void init() override
	{
		sha1algo.Check({
			{ "da39a3ee5e6b4b0d3255bfef95601890afd80709", "" },
			{ "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12", "The quick brown fox jumps over the lazy dog" },
		});
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
		sha3224algo.Check({
			{ "6b4e03423667dbb73b6e15454f0eb1abd4597f9a1b078e3f5b5a6bc7", "" },
			{ "d15dadceaa4d5d7bb3b48f446421d542e08ad8887305e28d58335795", "The quick brown fox jumps over the lazy dog" },
		});
		sha3256algo.Check({
			{ "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a", "" },
			{ "69070dda01975c8c120c3aada1b282394e7f032fa9cf32f4cb2259a0897dfc04", "The quick brown fox jumps over the lazy dog" },
		});
		sha3384algo.Check({
			{ "0c63a75b845e4f7d01107d852e4c2485c51a50aaaa94fc61995e71bbee983a2ac3713831264adb47fb6bd1e058d5f004", "" },
			{ "7063465e08a93bce31cd89d2e3ca8f602498696e253592ed26f07bf7e703cf328581e1471a7ba7ab119b1a9ebdf8be41", "The quick brown fox jumps over the lazy dog" },
		});
		sha3512algo.Check({
			{ "a69f73cca23a9ac5c8b567dc185a756e97c982164fe25859e0d1dcc1475c80a615b2123af1f5f94c11e3e9402c3ac558f500199d95b6d3e301758586281dcd26", "" },
			{ "01dedd5de4ef14642445ba5f5b97c15e47b9ad931326e4b0727cd94cefc44fff23f07bf543139939b49128caf436dc1bdee54fcb24023a08d9403f9b4bf0d450", "The quick brown fox jumps over the lazy dog" },
		});
	}
};

MODULE_INIT(ModuleHashGnuTLS)
