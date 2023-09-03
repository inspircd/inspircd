/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017, 2021-2022 Sadie Powell <sadie@witchery.services>
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

#include "compat.h"

/** Encapsulates a list of tokens in the format "* -FOO -BAR".*/
class CoreExport TokenList final
{
private:
	/** Whether this list includes all tokens by default. */
	bool permissive = false;

	/** Either the tokens to exclude if in permissive mode or the tokens to include if in strict mode. */
	insp::flat_set<std::string, irc::insensitive_swo> tokens;

public:
	/** Creates a new empty token list. */
	TokenList() = default;

	/** Creates a new token list from a list of tokens. */
	TokenList(const std::string& tokenlist);

	/** Adds a space-delimited list of tokens to the token list.
	 * @param tokenlist The list of space-delimited tokens to add.
	 */
	void AddList(const std::string& tokenlist);

	/** Adds a single token to the token list.
	 * @param token The token to add.
	 */
	void Add(const std::string& token);

	/** Removes all tokens from the token list. */
	void Clear();

	/** Determines whether the specified token exists in the token list.
	 * @param token The token to search for.
	 */
	bool Contains(const std::string& token) const;

	/** Removes the specified token from the token list.
	 * @param token The token to remove.
	 */
	void Remove(const std::string& token);

	/** Retrieves a string which represents the contents of this token list. */
	std::string ToString() const;

	/** Determines whether the specified token list contains the same tokens as this instance.
	 * @param other The tokenlist to compare against.
	 * @return True if the token lists are equal; otherwise, false.
	 */
	bool operator==(const TokenList& other) const;
};
