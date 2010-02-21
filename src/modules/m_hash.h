/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
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

class HashProvider : public DataProvider
{
 public:
	HashProvider(Module* mod, const std::string& Name) : DataProvider(mod, Name) {}
	virtual std::string sum(const std::string& data) = 0;
	inline std::string hexsum(const std::string& data)
	{
		return BinToHex(sum(data));
	}

	inline std::string b64sum(const std::string& data)
	{
		return BinToBase64(sum(data), NULL, 0);
	}

	/** Allows the IVs for the hash to be specified. As the choice of initial IV is
	 * important for the security of a hash, this should not be used except to
	 * maintain backwards compatability. This also allows you to change the hex
	 * sequence from its default of "0123456789abcdef", which does not improve the
	 * strength of the output, but helps confuse those attempting to implement it.
	 *
	 * Example:
	 * \code
	 * unsigned int iv[] = { 0xFFFFFFFF, 0x00000000, 0xAAAAAAAA, 0xCCCCCCCC };
	 * std::string result = Hash.sumIV(iv, "fedcba9876543210", "data");
	 * \endcode
	 */
	virtual std::string sumIV(unsigned int* IV, const char* HexMap, const std::string &sdata) = 0;
};

#endif

