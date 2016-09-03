/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005-2007 Craig Edwards <craigedwards@brainbox.cc>
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


#include "inspircd.h"

static const char hextable[] = "0123456789abcdef";

std::string BinToHex(const void* raw, size_t l)
{
	const char* data = static_cast<const char*>(raw);
	std::string rv;
	rv.reserve(l * 2);
	for (size_t i = 0; i < l; i++)
	{
		unsigned char c = data[i];
		rv.push_back(hextable[c >> 4]);
		rv.push_back(hextable[c & 0xF]);
	}
	return rv;
}

static const char b64table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string BinToBase64(const std::string& data_str, const char* table, char pad)
{
	if (!table)
		table = b64table;

	uint32_t buffer;
	uint8_t* data = (uint8_t*)data_str.data();
	std::string rv;
	size_t i = 0;
	while (i + 2 < data_str.length())
	{
		buffer = (data[i] << 16 | data[i+1] << 8 | data[i+2]);
		rv.push_back(table[0x3F & (buffer >> 18)]);
		rv.push_back(table[0x3F & (buffer >> 12)]);
		rv.push_back(table[0x3F & (buffer >>  6)]);
		rv.push_back(table[0x3F & (buffer >>  0)]);
		i += 3;
	}
	if (data_str.length() == i)
	{
		// no extra characters
	}
	else if (data_str.length() == i + 1)
	{
		buffer = data[i] << 16;
		rv.push_back(table[0x3F & (buffer >> 18)]);
		rv.push_back(table[0x3F & (buffer >> 12)]);
		if (pad)
		{
			rv.push_back(pad);
			rv.push_back(pad);
		}
	}
	else if (data_str.length() == i + 2)
	{
		buffer = (data[i] << 16 | data[i+1] << 8);
		rv.push_back(table[0x3F & (buffer >> 18)]);
		rv.push_back(table[0x3F & (buffer >> 12)]);
		rv.push_back(table[0x3F & (buffer >>  6)]);
		if (pad)
			rv.push_back(pad);
	}
	return rv;
}

std::string Base64ToBin(const std::string& data_str, const char* table)
{
	if (!table)
		table = b64table;

	int bitcount = 0;
	uint32_t buffer = 0;
	const char* data = data_str.c_str();
	std::string rv;
	while (true)
	{
		const char* find = strchr(table, *data++);
		if (!find || find >= table + 64)
			break;
		buffer = (buffer << 6) | (find - table);
		bitcount += 6;
		if (bitcount >= 8)
		{
			bitcount -= 8;
			rv.push_back((buffer >> bitcount) & 0xFF);
		}
	}
	return rv;
}

bool InspIRCd::TimingSafeCompare(const std::string& one, const std::string& two)
{
	if (one.length() != two.length())
		return false;

	unsigned int diff = 0;
	for (std::string::const_iterator i = one.begin(), j = two.begin(); i != one.end(); ++i, ++j)
	{
		unsigned char a = static_cast<unsigned char>(*i);
		unsigned char b = static_cast<unsigned char>(*j);
		diff |= a ^ b;
	}

	return (diff == 0);
}
