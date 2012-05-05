/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
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


#ifndef HASH_H
#define HASH_H

#include "modules.h"

class HashProvider : public DataProvider
{
 public:
	const unsigned int out_size;
	const unsigned int block_size;
	HashProvider(Module* mod, const std::string& Name, int osiz, int bsiz)
		: DataProvider(mod, Name), out_size(osiz), block_size(bsiz) {}
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

