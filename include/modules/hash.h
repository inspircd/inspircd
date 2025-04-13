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


#pragma once

#include "stringutils.h"
#include "utility/string.h"

namespace Hash
{
	class Context;
	class Provider;
	class ProviderRef;
	class HMACContext;
	class HMACProvider;

	/** Compares a password to a hashed password.
	 * @param password The hashed password.
	 * @param algorithm If non-empty then the algorithm the password is hashed with.
	 * @param value The value to check to see if the password is valid.
	 * @return True if the password is correct, otherwise, false.
	 */
	inline bool CheckPassword(const std::string& password, const std::string& algorithm, const std::string& value);

	/** Generates a hash-based message authentication code.
	 * @param prov The hash algorithm to hash with.
	 * @param key The secret key.
	 * @param data The data to hash.
	 */
	inline std::string HMAC(Hash::Provider* prov, const std::string& key, const std::string& data);
}


/** Base class for hash contexts. */
class Hash::Context
{
public:
	virtual ~Context() = default;

	/** Updates the hash context with the specified data.
	 * @param str The data to update the context with.
	 */
	inline void Update(const std::string& str)
	{
		Update(reinterpret_cast<const unsigned char *>(str.c_str()), str.length());
	}

	/** Updates the hash context with the specified data.
	 * @param data The data to update the context with.
	 * @param len The length of the data.
	 */
	virtual void Update(const unsigned char *data, size_t len) = 0;

	/** Finalises the hash context and returns the digest. */
	virtual std::string Finalize() = 0;
};

/** Provider of hash contexts. */
class Hash::Provider
	: public DataProvider
{
public:
	/** The byte size of the block cipher. */
	const size_t block_size;

	/** The byte size of the resulting digest. */
	const size_t digest_size;

	/** Creates a provider of hash contexts.
	 * @param mod The module that created this provider.
	 * @param algorithm The name of the hash algorithm.
	 * @param ds The byte size of the resulting digest or 0 if it is variable length.
	 * @param bs The byte size of the block cipher or 0 if not a block cipher.
	 */
	Provider(Module* mod, const std::string& algorithm, size_t ds = 0, size_t bs = 0)
		: DataProvider(mod, "hash/" + algorithm)
		, block_size(bs)
		, digest_size(ds)
	{
	}

	virtual ~Provider() = default;

	/** Checks whether a plain text value matches a hash created by this provider
	 * @param hash The hashed value to compare against.
	 * @param plain The plain text password to compare.
	 */
	virtual bool Compare(const std::string& hash, const std::string& plain)
	{
		return !hash.empty() && InspIRCd::TimingSafeCompare(hash, ToPrintable(Hash(plain)));
	}

	/** Called on initialising a hash provider to check it works properly.
	 * @param checks A map of known ciphertexts to plaintexts.
	 */
	inline void Check(const std::map<std::string, std::string>& checks)
	{
		for (const auto& [hash, plain] : checks)
		{
			if (!Compare(hash, plain))
				throw ModuleException(creator, "BUG: unable to generate {} hashes safely! Please report this!", GetAlgorithm());
		}

		ServerInstance->Logs.Debug("HASH", "The {} hash provider appears to be working correctly.", GetAlgorithm());
	}

	/** Creates a new hash context. */
	virtual std::unique_ptr<Context> CreateContext() = 0;

	/** Retrieves the name of the hash algorithm. */
	const char* GetAlgorithm() const
	{
		return name.c_str() + 5;
	}

	/** Quickly hashes the specified values and returns the digest. */
	template<typename... Args>
	std::string Hash(Args&& ...args)
	{
		auto context = CreateContext();
		context->Update(std::forward<Args>(args)...);
		return context->Finalize();
	}

	/** Determines whether this hash algorithm is a key derivation function. */
	auto IsKDF() const { return block_size == 0; }

	/** Converts a hash to its printable form. */
	virtual std::string ToPrintable(const std::string& hash)
	{
		return Hex::Encode(hash);
	}
};

/** Holds a dynamic reference to a hash provider. */
class Hash::ProviderRef final
	: public dynamic_reference_nocheck<Hash::Provider>
{
public:
	/** Holds a dynamic reference to a hash algorithm.
	 * @param mod The module that created this reference.
	 * @param algorithm The name of the hash algorithm.
	 */
	ProviderRef(Module* mod, const std::string& algorithm)
		: dynamic_reference_nocheck<Hash::Provider>(mod, "hash/" + algorithm)
	{
	}

	/** Retrieves the name of the referenced hash algorithm. */
	const char* GetAlgorithm() const
	{
		if (GetProvider().empty())
			return nullptr;
		return GetProvider().c_str() + 5;
	}
};

/** Provides a hash context for HMAC generation. */
class Hash::HMACContext final
	: public Hash::Context
{
private:
	/** The data which has been written to this context. */
	std::string buffer;

	/** The underlying hash provider. */
	Hash::ProviderRef provider;

	/** Generates a salt for the HMAC. */
	std::string GenerateSalt()
	{
		if (!provider || provider->IsKDF())
			return {};

		std::vector<char> salt(provider->digest_size);
		ServerInstance->GenRandom(salt.data(), salt.size());
		return std::string(salt.data(), salt.size());
	}

public:
	/** Creates HMAC hash context.
	 * @param prov The underlying hash provider.
	 */
	HMACContext(const Hash::ProviderRef& prov)
		: provider(prov)
	{
	}

	/** @copydoc Hash::HMACContext::Update */
	void Update(const unsigned char *data, size_t len) override
	{
		buffer.append(reinterpret_cast<const char *>(data), len);
	}

	/** @copydoc Hash::HMACContext::Finalize */
	std::string Finalize() override
	{
		if (!provider)
			return {}; // No underlying hash (should never happen).

		auto salt = this->GenerateSalt();
		if (salt.empty())
			return {};

		auto hash = Hash::HMAC(*provider, salt, buffer);
		if (hash.empty())
			return {};

		this->buffer.clear();
		return FMT::format("{}${}", Base64::Encode(salt), Base64::Encode(hash));
	}
};

/** Provides a hash provider for HMAC generation. */
class Hash::HMACProvider final
	: public Hash::Provider
{
private:
	/** The underlying hash provider. */
	Hash::ProviderRef provider;

public:
	/** Creates a provider of HMAC hash contexts.
	 * @param mod The module that created this provider.
	 * @param algorithm The name of the hash algorithm.
	 */
	HMACProvider(Module* mod, const std::string& algorithm)
		: Hash::Provider(mod, FMT::format("hmac-{}", algorithm))
		, provider(mod, algorithm)
	{
	}

	/** @copydoc Hash::Provider::Compare */
	bool Compare(const std::string& hash, const std::string& plain) override
	{
		if (!provider)
			return false; // No underlying hash (should never happen).

		auto sep = hash.find('$');
		if (sep == std::string::npos)
			return false; // Malformed hash.

		auto rawkey = Base64::Decode(hash.substr(0, sep));
		auto rawhash = Base64::Decode(hash.substr(sep + 1));

		auto expected = Hash::HMAC(*provider, rawkey, plain);
		return !expected.empty() && InspIRCd::TimingSafeCompare(rawhash, expected);
	}

	/** @copydoc Hash::Provider::CreateContext */
	std::unique_ptr<Context> CreateContext() override
	{
		return std::make_unique<HMACContext>(provider);
	}

	/** @copydoc Hash::Provider::ToPrintable */
	std::string ToPrintable(const std::string &hash) override
	{
		// We have no way to make this printable without the creating context
		// so we always return the printed form.
		return hash;
	}
};

inline bool Hash::CheckPassword(const std::string& password, const std::string& algorithm, const std::string& value)
{
	auto* hash = ServerInstance->Modules.FindDataService<Hash::Provider>("hash/" + algorithm);
	if (hash)
		return hash->Compare(password, value);

	// The hash algorithm wasn't provided by any modules. If its plain
	// text then we can check it internally.
	if (algorithm.empty() || insp::equalsci(algorithm, "plaintext"))
		return InspIRCd::TimingSafeCompare(password, value);

	ServerInstance->Logs.Debug("HASH", "Unable to check password hashed with an unknown algorithm: {}", algorithm);
	return false;
}

inline std::string Hash::HMAC(Hash::Provider* prov, const std::string& key, const std::string& data)
{
	if (!prov || prov->IsKDF())
		return {};

	auto keybuf = key.length() > prov->block_size ? prov->Hash(key) : key;
	keybuf.resize(prov->block_size);

	std::string hmac1;
	std::string hmac2;
	for (size_t i = 0; i < prov->block_size; ++i)
	{
		hmac1.push_back(static_cast<char>(keybuf[i] ^ 0x5C));
		hmac2.push_back(static_cast<char>(keybuf[i] ^ 0x36));
	}
	hmac2.append(data);
	hmac1.append(prov->Hash(hmac2));

	return prov->Hash(hmac1);
}
