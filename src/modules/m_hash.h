/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
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


#ifndef __HASH_H__
#define __HASH_H__

#include "modules.h"

#define SHA256_DIGEST_SIZE (256 / 8)
#define SHA256_BLOCK_SIZE  (512 / 8)

/** HashRequest is the base class used to send Hash requests to hashing.so.
 * You should not instantiate classes of type HashRequest directly, instead
 * you should instantiate classes of type HashResetRequest, HashSumRequest,
 * HashKeyRequest and HashHexRequest, shown below.
 */
class HashRequest : public Request
{
	/** The keys (IV) to use */
	unsigned int* keys;
	/** The output characters (hex sequence) to use */
	const char* outputs;
	/** The string to hash */
	std::string tohash;
 public:
	/** Initialize HashRequest as an Hash_RESET message */
	HashRequest(const char* req, Module* Me, Module* Target) : Request(Me, Target, req)
	{
	}

	/** Initialize HashRequest as an Hash_SUM message */
	HashRequest(Module* Me, Module* Target, const std::string &hashable) : Request(Me, Target, "SUM"), keys(NULL), outputs(NULL), tohash(hashable)
	{
	}

	/** Initialize HashRequest as an Hash_KEY message */
	HashRequest(Module* Me, Module* Target, unsigned int* k) : Request(Me, Target, "KEY"), keys(k), outputs(NULL), tohash("")
	{
	}

	/** Initialize HashRequest as an Hash_HEX message */
	HashRequest(Module* Me, Module* Target, const char* out) : Request(Me, Target, "HEX"), keys(NULL), outputs(out), tohash("")
	{
	}

	/** Get data to be hashed */
	std::string& GetHashData()
	{
		return tohash;
	}

	/** Get keys (IVs) to be used */
	unsigned int* GetKeyData()
	{
		return keys;
	}

	/** Get output characters (hex sequence) to be used */
	const char* GetOutputs()
	{
		return outputs;
	}
};

/** Send this class to the hashing module to query for its name.
 *
 * Example:
 * \code
 * cout << "Using hash algorithm: " << HashNameRequest(this, HashModule).Send();
 * \endcode
 */
class HashNameRequest : public HashRequest
{
 public:
	/** Initialize HashNameRequest for sending.
	 * @param Me A pointer to the sending module
	 * @param Target A pointer to the hashing module
	 */
	HashNameRequest(Module* Me, Module* Target) : HashRequest("NAME", Me, Target)
	{
	}
};

/** Send this class to the hashing module to reset the Hash module to a known state.
 * This will reset the IV to the defaults specified by the Hash spec,
 * and reset the hex sequence to "0123456789abcdef". It should be sent before
 * ANY other Request types.
 *
 * Example:
 * \code
 * // Reset the Hash module.
 * HashResetRequest(this, HashModule).Send();
 * \endcode
 */
class HashResetRequest : public HashRequest
{
 public:
	/** Initialize HashResetRequest for sending.
	 * @param Me A pointer to the sending module
	 * @param Target A pointer to the hashing module
	 */
	HashResetRequest(Module* Me, Module* Target) : HashRequest("RESET", Me, Target)
	{
	}
};

/** Send this class to the hashing module to HashSUM a std::string.
 * You should make sure you know the state of the module before you send this
 * class, e.g. by first sending an HashResetRequest class. The hash will be
 * returned when you call Send().
 *
 * Example:
 * \code
 * // ALWAYS ALWAYS reset first, or set your own IV and hex chars.
 * HashResetRequest(this, HashModule).Send();
 * // Get the Hash sum of the string 'doodads'.
 * std::string result = HashSumRequest(this, HashModule, "doodads").Send();
 * \endcode
 */
class HashSumRequest : public HashRequest
{
 public:
	/** Initialize HashSumRequest for sending.
	 * @param Me A pointer to the sending module
	 * @param Target A pointer to the hashing module
	 * @param data The data to be hashed
	 */
	HashSumRequest(Module* Me, Module* Target, const std::string &sdata) : HashRequest(Me, Target, sdata)
	{
	}
};

/** Send this class to hashing module to change the IVs (keys) to use for hashing.
 * You should make sure you know the state of the module before you send this
 * class, e.g. by first sending an HashResetRequest class. The default values for
 * the IV's are those specified in the Hash specification. Only in very special
 * circumstances should you need to change the IV's (see for example m_cloaking.cpp)
 *
 * Example:
 * \code
 * unsigned int iv[] = { 0xFFFFFFFF, 0x00000000, 0xAAAAAAAA, 0xCCCCCCCC };
 * HashKeyRequest(this, HashModule, iv);
 * \endcode
 */
class HashKeyRequest : public HashRequest
{
 public:
	/** Initialize HashKeyRequest for sending.
	 * @param Me A pointer to the sending module
	 * @param Target A pointer to the hashing module
	 * @param data The new IV's. This should be an array of exactly four 32 bit values.
	 * On 64-bit architectures, the upper 32 bits of the values will be discarded.
	 */
	HashKeyRequest(Module* Me, Module* Target, unsigned int* sdata) : HashRequest(Me, Target, sdata)
	{
	}
};

/** Send this class to the hashing module to change the hex sequence to use for generating the returned value.
 * You should make sure you know the state of the module before you send this
 * class, e.g. by first sending an HashResetRequest class. The default value for
 * the hex sequence is "0123456789abcdef". Only in very special circumstances should
 * you need to change the hex sequence (see for example m_cloaking.cpp).
 *
 * Example:
 * \code
 * static const char tab[] = "fedcba9876543210";
 * HashHexRequest(this, HashModule, tab);
 * \endcode
 */
class HashHexRequest : public HashRequest
{
 public:
	/** Initialize HashHexRequest for sending.
	 * @param Me A pointer to the sending module
	 * @param Target A pointer to the hashing module
	 * @param data The hex sequence to use. This should contain exactly 16 ASCII characters,
	 * terminated by a NULL char.
	 */
	HashHexRequest(Module* Me, Module* Target, const char* sdata) : HashRequest(Me, Target, sdata)
	{
	}
};

#endif

