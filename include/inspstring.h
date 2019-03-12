/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#pragma once

// This (config) is needed as inspstring doesn't pull in the central header
#include "config.h"
#include <cstring>

/** Sets ret to the formated string. last is the last parameter before ..., and format is the format in printf-style */
#define VAFORMAT(ret, last, format) \
	do { \
	va_list _vaList; \
	va_start(_vaList, last); \
	ret.assign(InspIRCd::Format(_vaList, format)); \
	va_end(_vaList); \
	} while (false);

/** Compose a hex string from raw data.
 * @param raw The raw data to compose hex from (can be NULL if rawsize is 0)
 * @param rawsize The size of the raw data buffer
 * @return The hex string
 */
CoreExport std::string BinToHex(const void* raw, size_t rawsize);

/** Base64 encode */
CoreExport std::string BinToBase64(const std::string& data, const char* table = NULL, char pad = 0);
/** Base64 decode */
CoreExport std::string Base64ToBin(const std::string& data, const char* table = NULL);

/** Compose a hex string from the data in a std::string.
 * @param data The data to compose hex from
 * @return The hex string.
 */
inline std::string BinToHex(const std::string& data)
{
	return BinToHex(data.data(), data.size());
}
