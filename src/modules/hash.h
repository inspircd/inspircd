/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
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

class HashProvider : public DataProvider
{
 public:
	const unsigned int out_size;
	const unsigned int block_size;
	HashProvider(Module* mod, const std::string& Name, int osiz, int bsiz)
		: DataProvider(mod, Name), out_size(osiz), block_size(bsiz) {}
	/**
	 * Compute a checksum of the given data
	 * @param data The data to checksum
	 * @param IV The initial state of the hash, if different from the specification.
	 */
	virtual std::string sum(const std::string& data, const unsigned int* IV = 0) = 0;
	inline std::string hexsum(const std::string& data)
	{
		return BinToHex(sum(data));
	}

	inline std::string b64sum(const std::string& data)
	{
		return BinToBase64(sum(data), NULL, 0);
	}

	/** HMAC algorithm, RFC 2104 */
	std::string hmac(const std::string& key, const std::string& msg)
	{
		std::string hmac1, hmac2;
		std::string kbuf = key.length() > block_size ? sum(key) : key;
		kbuf.resize(block_size);

		for (size_t n = 0; n < block_size; n++)
		{
			hmac1.push_back(static_cast<char>(kbuf[n] ^ 0x5C));
			hmac2.push_back(static_cast<char>(kbuf[n] ^ 0x36));
		}
		hmac2.append(msg);
		hmac1.append(sum(hmac2));
		return sum(hmac1);
	}
};

#endif

