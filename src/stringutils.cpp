/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2017, 2021-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2010 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
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

#include <fmt/format.cc>

#include "inspircd.h"
#include "stringutils.h"
#include "utility/string.h"

std::string Percent::Encode(const void* data, size_t length, const char* table, bool upper)
{
	if (!table)
		table = Percent::TABLE;

	// Preallocate the output buffer to avoid constant reallocations.
	std::string buffer;
	buffer.reserve(length * 3);

	const char* hex_table = upper ? Hex::TABLE_UPPER : Hex::TABLE_LOWER;
	const unsigned char* udata = reinterpret_cast<const unsigned char*>(data);
	for (size_t idx = 0; idx < length; ++idx)
	{
		unsigned char chr = udata[idx];
		if (strchr(table, chr))
		{
			// The character is on the safe list; push it as is.
			buffer.push_back(chr);
		}
		else
		{
			// The character is not on the safe list; percent encode it.
			buffer.push_back('%');
			buffer.push_back(hex_table[chr >> 4]);
			buffer.push_back(hex_table[chr & 15]);
		}
	}

	return buffer;
}

std::string Percent::Decode(const void* data, size_t length)
{
	// Preallocate the output buffer to avoid constant reallocations.
	std::string buffer;
	buffer.reserve(length * 3);

	const char* cdata = reinterpret_cast<const char*>(data);
	for (size_t idx = 0; idx < length; ++idx)
	{
		if (cdata[idx] == '%')
		{
			// Percent encoding encodes two octets into 1-2 characters.
			const char octet1 = ++idx < length ? toupper(cdata[idx]) : 0;
			const char octet2 = ++idx < length ? toupper(cdata[idx]) : 0;

			const char* table1 = strchr(Hex::TABLE_UPPER, octet1);
			const char* table2 = strchr(Hex::TABLE_UPPER, octet2);

			char pair = ((table1 ? table1 - Hex::TABLE_UPPER : 0) << 4) + (table2 ? table2 - Hex::TABLE_UPPER : 0);
			buffer.push_back(pair);
		}
		else
		{
			buffer.push_back(cdata[idx]);
		}
	}

	return buffer;
}

std::string Hex::Encode(const void* data, size_t length, const char* table, char separator)
{
	if (!table)
		table = Hex::TABLE_LOWER;

	// Preallocate the output buffer to avoid constant reallocations.
	std::string buffer;
	buffer.reserve((length * 2) + (!!separator * length));

	const unsigned char* udata = reinterpret_cast<const unsigned char*>(data);
	for (size_t idx = 0; idx < length; ++idx)
	{
		if (idx && separator)
			buffer.push_back(separator);

		const unsigned char chr = udata[idx];
		buffer.push_back(table[chr >> 4]);
		buffer.push_back(table[chr & 15]);
	}

	return buffer;
}

std::string Hex::Decode(const void* data, size_t length, const char* table, char separator)
{
	if (!table)
		table = Hex::TABLE_LOWER;

	// The size of each hex segment.
	size_t segment = (separator ? 3 : 2);

	// Preallocate the output buffer to avoid constant reallocations.
	std::string buffer;
	buffer.reserve(length / segment);

	const char* cdata = static_cast<const char*>(data);
	for (size_t idx = 0; idx + 1 < length; idx += segment)
	{
		// Attempt to find the octets in the table.
		const char* table1 = strchr(table, cdata[idx]);
		const char* table2 = strchr(table, cdata[idx + 1]);

		char pair = ((table1 ? table1 - table : 0) << 4) + (table2 ? table2 - table : 0);
		buffer.push_back(pair);
	}

	return buffer;
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

	static constexpr size_t padding_count[] = { 0, 2, 1 };
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
		current_bits = (current_bits << 6) | uint32_t(chr - table);
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

std::string Template::Replace(const std::string& str, const VariableMap& vars)
{
	std::string out;
	out.reserve(str.length());

	for (size_t idx = 0; idx < str.size(); ++idx)
	{
		if (str[idx] != '%')
		{
			out.push_back(str[idx]);
			continue;
		}

		for (size_t endidx = idx + 1; endidx < str.size(); ++endidx)
		{
			if (str[endidx] == '%')
			{
				if (endidx - idx == 1)
				{
					// foo%%bar is an escape of foo%bar
					out.push_back('%');
					idx = endidx;
					break;
				}

				auto var = vars.find(str.substr(idx + 1, endidx - idx - 1));
				if (var != vars.end())
				{
					// We have a variable, replace it in the string.
					out.append(var->second);
				}

				idx = endidx;
				break;
			}
		}
	}

	return out;
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

TokenList::TokenList(const std::string& tokenlist)
{
	AddList(tokenlist);
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
	if (permissive)
	{
		// If the token list is in permissive mode then the tokens are a list
		// of disallowed tokens.
		std::string buffer("*");
		for (const auto& token : tokens)
			buffer.append(" -").append(token);
		return buffer;
	}

	// If the token list is not in permissive mode then the token list is just
	// a list of allowed tokens.
	return insp::join(tokens);
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
