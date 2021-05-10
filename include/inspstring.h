/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2007, 2010 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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

#include <cstring>

/** Sets ret to the formatted string. last is the last parameter before ..., and format is the format in printf-style */
#define VAFORMAT(ret, last, format) \
	do { \
	va_list _vaList; \
	va_start(_vaList, last); \
	ret.assign(InspIRCd::Format(_vaList, format)); \
	va_end(_vaList); \
	} while (false)


namespace Base64
{
	/** The default table used when handling Base64-encoded strings. */
	inline const char* TABLE = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	/** Decodes a Base64-encoded byte array.
	 * @param data The byte array to decode from.
	 * @param length The length of the byte array.
	 * @param table The index table to use for decoding.
	 * @return The decoded form of the specified data.
	 */
	CoreExport std::string Decode(const void* data, size_t length, const char* table = nullptr);

	/** Decodes a Base64-encoded string.
	 * @param data The string to decode from.
	 * @param table The index table to use for decoding.
	 * @return The decoded form of the specified data.
	 */
	inline std::string Decode(const std::string& data, const char* table = nullptr)
	{
		return Decode(data.c_str(), data.length(), table);
	}

	/** Encodes a byte array using Base64.
	 * @param data The byte array to encode from.
	 * @param length The length of the byte array.
	 * @param table The index table to use for encoding.
	 * @param padding If non-zero then the character to pad encoded strings with.
	 * @return The encoded form of the specified data.
	 */
	CoreExport std::string Encode(const void* data, size_t length, const char* table = nullptr, char padding = 0);

	/** Encodes a string using Base64.
	 * @param data The string to encode from.
	 * @param table The index table to use for encoding.
	 * @param padding If non-zero then the character to pad encoded strings with.
	 * @return The encoded form of the specified data.
	 */
	inline std::string Encode(const std::string& data, const char* table = nullptr, char padding = 0)
	{
		return Encode(data.c_str(), data.length(), table, padding);
	}
}

/** Compose a hex string from raw data.
 * @param raw The raw data to compose hex from (can be NULL if rawsize is 0)
 * @param rawsize The size of the raw data buffer
 * @return The hex string
 */
CoreExport std::string BinToHex(const void* raw, size_t rawsize);

/** Compose a hex string from the data in a std::string.
 * @param data The data to compose hex from
 * @return The hex string.
 */
inline std::string BinToHex(const std::string& data)
{
	return BinToHex(data.data(), data.size());
}
