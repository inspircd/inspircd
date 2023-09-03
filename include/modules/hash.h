/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2013 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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

class HashProvider
	: public DataProvider
{
public:
	const unsigned int out_size;
	const unsigned int block_size;
	HashProvider(Module* mod, const std::string& Name, unsigned int osiz = 0, unsigned int bsiz = 0)
		: DataProvider(mod, "hash/" + Name)
		, out_size(osiz)
		, block_size(bsiz)
	{
	}

	virtual std::string GenerateRaw(const std::string& data) = 0;

	virtual std::string ToPrintable(const std::string& raw)
	{
		return Hex::Encode(raw);
	}

	virtual bool Compare(const std::string& input, const std::string& hash)
	{
		return InspIRCd::TimingSafeCompare(Generate(input), hash);
	}

	std::string Generate(const std::string& data)
	{
		return ToPrintable(GenerateRaw(data));
	}

	/** HMAC algorithm, RFC 2104 */
	std::string hmac(const std::string& key, const std::string& msg)
	{
		std::string hmac1, hmac2;
		std::string kbuf = key.length() > block_size ? GenerateRaw(key) : key;
		kbuf.resize(block_size);

		for (size_t n = 0; n < block_size; n++)
		{
			hmac1.push_back(static_cast<char>(kbuf[n] ^ 0x5C));
			hmac2.push_back(static_cast<char>(kbuf[n] ^ 0x36));
		}
		hmac2.append(msg);
		hmac1.append(GenerateRaw(hmac2));
		return GenerateRaw(hmac1);
	}

	bool IsKDF() const
	{
		return (!block_size);
	}
};
