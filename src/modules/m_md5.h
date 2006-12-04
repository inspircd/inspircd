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

#ifndef __MD5_H__
#define __MD5_H__

#include "modules.h"

/* MD5Request is the base class used to send MD5 requests to m_md5.so.
 * You should not instantiate classes of type MD5Request directly, instead
 * you should instantiate classes of type MD5ResetRequest, MD5SumRequest,
 * MD5KeyRequest and MD5HexRequest, shown below.
 */
class MD5Request : public Request
{
	/** The keys (IV) to use */
	unsigned int* keys;
	/** The output characters (hex sequence) to use */
	const char* outputs;
	/** The string to hash */
	std::string tohash;
 public:
	/** Initialize MD5Request as an MD5_RESET message */
	MD5Request(Module* Me, Module* Target) : Request(Me, Target, "MD5_RESET")
	{
	}

	/** Initialize MD5Request as an MD5_SUM message */
	MD5Request(Module* Me, Module* Target, const std::string &hashable) : Request(Me, Target, "MD5_SUM"), keys(NULL), outputs(NULL), tohash(hashable)
	{
	}

	/** Initialize MD5Request as an MD5_KEY message */
	MD5Request(Module* Me, Module* Target, unsigned int* k) : Request(Me, Target, "MD5_KEY"), keys(k), outputs(NULL), tohash("")
	{
	}

	/** Initialize MD5Request as an MD5_HEX message */
	MD5Request(Module* Me, Module* Target, const char* out) : Request(Me, Target, "MD5_HEX"), keys(NULL), outputs(out), tohash("")
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

/** Send this class to m_md5.so to reset the MD5 module to a known state.
 * This will reset the IV to the defaults specified by the MD5 spec,
 * and reset the hex sequence to "0123456789abcdef". It should be sent before
 * ANY other Request types.
 *
 * Example:
 * \code
 * // Reset the MD5 module.
 * MD5ResetRequest(this, MD5Module).Send();
 * \endcode
 */
class MD5ResetRequest : public MD5Request
{
 public:
	/** Initialize MD5ResetRequest for sending.
	 * @param Me A pointer to the sending module
	 * @param Target A pointer to the m_md5.so module
	 */
	MD5ResetRequest(Module* Me, Module* Target) : MD5Request(Me, Target)
	{
	}
};

/** Send this class to m_md5.so to MD5SUM a std::string.
 * You should make sure you know the state of the module before you send this
 * class, e.g. by first sending an MD5ResetRequest class. The hash will be
 * returned when you call Send().
 *
 * Example:
 * \code
 * // ALWAYS ALWAYS reset first, or set your own IV and hex chars.
 * MD5ResetRequest(this, MD5Module).Send();
 * // Get the MD5 sum of the string 'doodads'.
 * std::string result = MD5SumRequest(this, MD5Module, "doodads").Send();
 * \endcode
 */
class MD5SumRequest : public MD5Request
{
 public:
	/** Initialize MD5SumRequest for sending.
	 * @param Me A pointer to the sending module
	 * @param Target A pointer to the m_md5.so module
	 * @param data The data to be hashed
	 */
	MD5SumRequest(Module* Me, Module* Target, const std::string &data) : MD5Request(Me, Target, data)
	{
	}
};

/** Send this class to m_md5.so to change the IVs (keys) to use for hashing.
 * You should make sure you know the state of the module before you send this
 * class, e.g. by first sending an MD5ResetRequest class. The default values for
 * the IV's are those specified in the MD5 specification. Only in very special
 * circumstances should you need to change the IV's (see for example m_cloaking.cpp)
 *
 * Example:
 * \code
 * unsigned int iv[] = { 0xFFFFFFFF, 0x00000000, 0xAAAAAAAA, 0xCCCCCCCC };
 * MD5KeyRequest(this, MD5Module, iv);
 * \endcode
 */
class MD5KeyRequest : public MD5Request
{
 public:
	/** Initialize MD5KeyRequest for sending.
	 * @param Me A pointer to the sending module
	 * @param Target A pointer to the m_md5.so module
	 * @param data The new IV's. This should be an array of exactly four 32 bit values.
	 * On 64-bit architectures, the upper 32 bits of the values will be discarded.
	 */
	MD5KeyRequest(Module* Me, Module* Target, unsigned int* data) : MD5Request(Me, Target, data)
	{
	}
};

/** Send this class to m_md5.so to change the hex sequence to use for generating the returned value.
 * You should make sure you know the state of the module before you send this
 * class, e.g. by first sending an MD5ResetRequest class. The default value for
 * the hex sequence is "0123456789abcdef". Only in very special circumstances should
 * you need to change the hex sequence (see for example m_cloaking.cpp).
 *
 * Example:
 * \code
 * static const char tab[] = "fedcba9876543210";
 * MD5HexRequest(this, MD5Module, tab);
 * \endcode
 */
class MD5HexRequest : public MD5Request
{
 public:
	/** Initialize MD5HexRequest for sending.
	 * @param Me A pointer to the sending module
	 * @param Target A pointer to the m_md5.so module
	 * @param data The hex sequence to use. This should contain exactly 16 ASCII characters,
	 * terminated by a NULL char.
	 */
	MD5HexRequest(Module* Me, Module* Target, const char* data) : MD5Request(Me, Target, data)
	{
	}
};

#endif

