/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __HASH_H__
#define __HASH_H__

#include "modules.h"

#define SHA256_DIGEST_SIZE (256 / 8)
#define SHA256_BLOCK_SIZE  (512 / 8)

/** Query a hash algorithm's name
 *
 * Example:
 * \code
 * cout << "Using hash algorithm: " << HashNameRequest(this, HashModule).response;
 * \endcode
 */
struct HashNameRequest : public Request
{
	std::string response;
	HashNameRequest(Module* Me, Module* Target) : Request(Me, Target, "NAME")
	{
		Send();
	}
};

/** Send this class to the hashing module to HashSUM a std::string.
 *
 * Example:
 * \code
 * // Get the Hash sum of the string 'doodads'.
 * std::string result = HashRequest(this, HashModule, "doodads").result;
 * \endcode
 */
struct HashRequest : public Request
{
	const std::string data;
	std::string binresult;
	/** Initialize HashSumRequest for sending.
	 * @param Me A pointer to the sending module
	 * @param Target A pointer to the hashing module
	 * @param data The data to be hashed
	 */
	HashRequest(Module* Me, Module* Target, const std::string &sdata)
		: Request(Me, Target, "HASH"), data(sdata)
	{
		Send();
	}
	inline std::string hex()
	{
		return BinToHex(binresult);
	}
};

/** Allows the IVs for the hash to be specified. As the choice of initial IV is
 * important for the security of a hash, this should not be used except to
 * maintain backwards compatability. This also allows you to change the hex
 * sequence from its default of "0123456789abcdef", which does not improve the
 * strength of the output, but helps confuse those attempting to implement it.
 *
 * Only m_md5 implements this request; only m_cloaking should use it.
 *
 * Example:
 * \code
 * unsigned int iv[] = { 0xFFFFFFFF, 0x00000000, 0xAAAAAAAA, 0xCCCCCCCC };
 * std::string result = HashRequestIV(this, HashModule, iv, "0123456789abcdef", "data").result;
 * \endcode
 */
struct HashRequestIV : public Request
{
	unsigned int* iv;
	const char* map;
	std::string result;
	const std::string data;
	HashRequestIV(Module* Me, Module* Target, unsigned int* IV, const char* HexMap, const std::string &sdata)
		: Request(Me, Target, "HASH-IV"), iv(IV), map(HexMap), data(sdata)
	{
		Send();
	}
};

#endif

