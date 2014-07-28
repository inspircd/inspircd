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


#pragma once

#include "modules.h"

class HashProvider : public DataProvider
{
 public:
	enum HashType
	{
		HASH_RAW,
		HASH_PRINTABLE
	};

	const unsigned int out_size;
	const unsigned int block_size;
	HashProvider(Module* mod, const std::string& Name, unsigned int osiz = 0, unsigned int bsiz = 0)
		: DataProvider(mod, "hash/" + Name), out_size(osiz), block_size(bsiz)
	{
	}

	virtual std::string Generate(const std::string& data, const HashType type = HASH_PRINTABLE) = 0;
	virtual std::string RAW(const std::string& raw) = 0;

	virtual bool Compare(const std::string& string, const std::string& hash)
	{
		return Generate(string) == hash;
	}

	/** HMAC algorithm, RFC 2104 */
	std::string HMAC(const std::string& key, const std::string& msg)
	{
		std::string hmac1, hmac2;
		std::string kbuf = key.length() > block_size ? Generate(key, HashProvider::HASH_RAW) : key;
		kbuf.resize(block_size);

		for (size_t n = 0; n < block_size; n++)
		{
			hmac1.push_back(static_cast<char>(kbuf[n] ^ 0x5C));
			hmac2.push_back(static_cast<char>(kbuf[n] ^ 0x36));
		}
		hmac2.append(msg);
		hmac1.append(Generate(hmac2, HashProvider::HASH_RAW));
		return Generate(hmac1, HashProvider::HASH_RAW);
	}

	bool IsKDF() const
	{
		return !block_size;
	}
};
