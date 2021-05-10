/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2010 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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

std::string Base64::Encode(const void* data, size_t length, const char* table, char padding)
{
	// Use the default table if one is not specified.
	if (!table)
		table = Base64::TABLE;

	// Preallocate the output buffer to avoid constant reallocations.
	std::string buffer;
	buffer.reserve(4 * ((length + 2) / 3));

	const uint8_t* udata = static_cast<const uint8_t*>(data);
	for (size_t idx = 0; idx < length; )
	{
		// Base64 encodes three octets into four characters.
		uint32_t octet1 = idx < length ? udata[idx++] : 0;
		uint32_t octet2 = idx < length ? udata[idx++] : 0;
		uint32_t octet3 = idx < length ? udata[idx++] : 0;
		uint32_t triple = (octet1 << 16) + (octet2 << 8) + octet3;

		buffer.push_back(table[(triple >> 3 * 6) & 63]);
		buffer.push_back(table[(triple >> 2 * 6) & 63]);
		buffer.push_back(table[(triple >> 1 * 6) & 63]);
		buffer.push_back(table[(triple >> 0 * 6) & 63]);
	}

	static const size_t padding_count[] = { 0, 2, 1 };
	if (padding)
	{
		// Replace any trailing characters with padding.
		for (size_t idx = 0; idx < padding_count[length % 3]; ++idx)
			buffer[buffer.length() - 1 - idx] = '=';
	}
	else
	{
		// Remove any trailing characters.
		buffer.erase(buffer.length() - padding_count[length % 3]);
	}

	return buffer;
}

std::string Base64::Decode(const void* data, size_t length, const char* table)
{
	if (!table)
		table = Base64::TABLE;

	// Preallocate the output buffer to avoid constant reallocations.
	std::string buffer;
	buffer.reserve((length / 4) * 3);

	uint32_t current_bits = 0;
	size_t seen_bits = 0;

	const char* cdata = static_cast<const char*>(data);
	for (size_t idx = 0; idx < length; ++idx)
	{
		// Attempt to find the octet in the table.
		const char* chr = strchr(table, cdata[idx]);
		if (!chr)
			continue; // Skip invalid octets.

		// Add the bits for this octet to the active buffer.
		current_bits = (current_bits << 6) | (chr - table);
		seen_bits += 6;

		if (seen_bits >= 8)
		{
			// We have seen an entire octet; add it to the buffer.
			seen_bits -= 8;
			buffer.push_back((current_bits >> seen_bits) & 0xFF);
		}
	}

	return buffer;
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

void TokenList::AddList(const std::string& tokenlist)
{
	std::string token;
	irc::spacesepstream tokenstream(tokenlist);
	while (tokenstream.GetToken(token))
	{
		if (token[0] == '-')
			Remove(token.substr(1));
		else
			Add(token);
	}
}
void TokenList::Add(const std::string& token)
{
	// If the token is empty or contains just whitespace it is invalid.
	if (token.empty() || token.find_first_not_of(" \t") == std::string::npos)
		return;

	// If the token is a wildcard entry then permissive mode has been enabled.
	if (token == "*")
	{
		permissive = true;
		tokens.clear();
		return;
	}

	// If we are in permissive mode then remove the token from the token list.
	// Otherwise, add it to the token list.
	if (permissive)
		tokens.erase(token);
	else
		tokens.insert(token);
}

void TokenList::Clear()
{
	permissive = false;
	tokens.clear();
}

bool TokenList::Contains(const std::string& token) const
{
	// If we are in permissive mode and the token is in the list
	// then we don't have it.
	if (permissive && tokens.find(token) != tokens.end())
		return false;

	// If we are not in permissive mode and the token is not in
	// the list then we don't have it.
	if (!permissive && tokens.find(token) == tokens.end())
		return false;

	// We have the token!
	return true;
}

void TokenList::Remove(const std::string& token)
{
	// If the token is empty or contains just whitespace it is invalid.
	if (token.empty() || token.find_first_not_of(" \t") == std::string::npos)
		return;

	// If the token is a wildcard entry then permissive mode has been disabled.
	if (token == "*")
	{
		permissive = false;
		tokens.clear();
		return;
	}

	// If we are in permissive mode then add the token to the token list.
	// Otherwise, remove it from the token list.
	if (permissive)
		tokens.insert(token);
	else
		tokens.erase(token);
}

std::string TokenList::ToString() const
{
	std::string buffer(permissive ? "* " : "-* ");
	buffer.append(stdalgo::string::join(tokens));
	return buffer;
}

bool TokenList::operator==(const TokenList& other) const
{
	// Both sets must be in the same mode to be equal.
	if (permissive != other.permissive)
		return false;

	// Both sets must be the same size to be equal.
	if (tokens.size() != other.tokens.size())
		return false;

	for (const auto& token : tokens)
	{
		// Both sets must contain the same tokens to be equal.
		if (other.tokens.find(token) == other.tokens.end())
			return false;
	}

	return true;
}
