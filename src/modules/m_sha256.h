/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __SHA256_H__
#define __SHA256_H__

#include "modules.h"

#define SHA256_DIGEST_SIZE (256 / 8)
#define SHA256_BLOCK_SIZE  (512 / 8)

/** SHA256Request is the base class used to send SHA256 requests to m_sha256.so.
 * You should not instantiate classes of type SHA256Request directly, instead
 * you should instantiate classes of type SHA256ResetRequest, SHA256SumRequest,
 * SHA256KeyRequest and SHA256HexRequest, shown below.
 */
class SHA256Request : public Request
{
	/** The keys (IV) to use */
	unsigned int* keys;
	/** The output characters (hex sequence) to use */
	const char* outputs;
	/** The string to hash */
	std::string tohash;
 public:
	/** Initialize SHA256Request as an SHA256_RESET message */
	SHA256Request(Module* Me, Module* Target) : Request(Me, Target, "SHA256_RESET")
	{
	}

	/** Initialize SHA256Request as an SHA256_SUM message */
	SHA256Request(Module* Me, Module* Target, const std::string &hashable) : Request(Me, Target, "SHA256_SUM"), keys(NULL), outputs(NULL), tohash(hashable)
	{
	}

	/** Initialize SHA256Request as an SHA256_KEY message */
	SHA256Request(Module* Me, Module* Target, unsigned int* k) : Request(Me, Target, "SHA256_KEY"), keys(k), outputs(NULL), tohash("")
	{
	}

	/** Initialize SHA256Request as an SHA256_HEX message */
	SHA256Request(Module* Me, Module* Target, const char* out) : Request(Me, Target, "SHA256_HEX"), keys(NULL), outputs(out), tohash("")
	{
	}

	/** Get data to be hashed */
	const char* GetHashData()
	{
		return tohash.c_str();
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

/** Send this class to m_sha256.so to reset the SHA256 module to a known state.
 * This will reset the IV to the defaults specified by the SHA256 spec,
 * and reset the hex sequence to "0123456789abcdef". It should be sent before
 * ANY other Request types.
 *
 * Example:
 * \code
 * // Reset the SHA256 module.
 * SHA256ResetRequest(this, SHA256Module).Send();
 * \endcode
 */
class SHA256ResetRequest : public SHA256Request
{
 public:
	/** Initialize SHA256ResetRequest for sending.
	 * @param Me A pointer to the sending module
	 * @param Target A pointer to the m_sha256.so module
	 */
	SHA256ResetRequest(Module* Me, Module* Target) : SHA256Request(Me, Target)
	{
	}
};

/** Send this class to m_sha256.so to SHA256SUM a std::string.
 * You should make sure you know the state of the module before you send this
 * class, e.g. by first sending an SHA256ResetRequest class. The hash will be
 * returned when you call Send().
 *
 * Example:
 * \code
 * // ALWAYS ALWAYS reset first, or set your own IV and hex chars.
 * SHA256ResetRequest(this, SHA256Module).Send();
 * // Get the SHA256 sum of the string 'doodads'.
 * std::string result = SHA256SumRequest(this, SHA256Module, "doodads").Send();
 * \endcode
 */
class SHA256SumRequest : public SHA256Request
{
 public:
	/** Initialize SHA256SumRequest for sending.
	 * @param Me A pointer to the sending module
	 * @param Target A pointer to the m_sha256.so module
	 * @param data The data to be hashed
	 */
	SHA256SumRequest(Module* Me, Module* Target, const std::string &data) : SHA256Request(Me, Target, data)
	{
	}
};

/** Send this class to m_sha256.so to change the IVs (keys) to use for hashing.
 * You should make sure you know the state of the module before you send this
 * class, e.g. by first sending an SHA256ResetRequest class. The default values for
 * the IV's are those specified in the SHA256 specification. Only in very special
 * circumstances should you need to change the IV's (see for example m_cloaking.cpp)
 *
 * Example:
 * \code
 * unsigned int iv[] = { 0xFFFFFFFF, 0x00000000, 0xAAAAAAAA, 0xCCCCCCCC };
 * SHA256KeyRequest(this, SHA256Module, iv);
 * \endcode
 */
class SHA256KeyRequest : public SHA256Request
{
 public:
	/** Initialize SHA256KeyRequest for sending.
	 * @param Me A pointer to the sending module
	 * @param Target A pointer to the m_sha256.so module
	 * @param data The new IV's. This should be an array of exactly four 32 bit values.
	 * On 64-bit architectures, the upper 32 bits of the values will be discarded.
	 */
	SHA256KeyRequest(Module* Me, Module* Target, unsigned int* data) : SHA256Request(Me, Target, data)
	{
	}
};

/** Send this class to m_sha256.so to change the hex sequence to use for generating the returned value.
 * You should make sure you know the state of the module before you send this
 * class, e.g. by first sending an SHA256ResetRequest class. The default value for
 * the hex sequence is "0123456789abcdef". Only in very special circumstances should
 * you need to change the hex sequence (see for example m_cloaking.cpp).
 *
 * Example:
 * \code
 * static const char tab[] = "fedcba9876543210";
 * SHA256HexRequest(this, SHA256Module, tab);
 * \endcode
 */
class SHA256HexRequest : public SHA256Request
{
 public:
	/** Initialize SHA256HexRequest for sending.
	 * @param Me A pointer to the sending module
	 * @param Target A pointer to the m_sha256.so module
	 * @param data The hex sequence to use. This should contain exactly 16 ASCII characters,
	 * terminated by a NULL char.
	 */
	SHA256HexRequest(Module* Me, Module* Target, const char* data) : SHA256Request(Me, Target, data)
	{
	}
};

#endif

