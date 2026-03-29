/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2026 Sadie Powell <sadie@witchery.services>
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

namespace insp
{
	template<typename, typename>
	struct casemapped_equals_obj;
	template<typename>
	struct casemapped_hash_obj;
	template<typename, typename>
	struct casemapped_less_obj;

	/** Holds a list of characters and the value they map to. */
	using casemap = unsigned char;

	/** A flat map where the key is a case insensitive string using the IRC casemapping. */
	template<typename Value, typename Key = std::string>
	using casemapped_flat_map = insp::flat_map<std::string, Value, casemapped_less_obj<Key, Key>>;

	/** A flat multimap where the key is a case insensitive string using the IRC casemapping. */
	template<typename Value, typename Key = std::string>
	using casemapped_flat_multimap = insp::flat_multimap<std::string, Value, casemapped_less_obj<Key, Key>>;

	/** A flat set where the key is a case insensitive string using the IRC casemapping. */
	template<typename Key = std::string>
	using casemapped_flat_set = insp::flat_set<Key, casemapped_less_obj<Key, Key>>;

	/** A map where the key is a case insensitive string using the IRC casemapping. */
	template<typename Value, typename Key = std::string>
	using casemapped_map = std::map<Key, Value, casemapped_less_obj<Key, Key>>;

	/** A flat multimap where the key is a case insensitive string using the IRC casemapping. */
	template<typename Value, typename Key = std::string>
	using casemapped_multimap = std::multimap<Key, Value, casemapped_less_obj<Key, Key>>;

	/** An unordered map where the key is a case insensitive string using the IRC casemapping. */
	template<typename Value, typename Key = std::string>
	using casemapped_unordered_map = std::unordered_map<Key, Value, casemapped_hash_obj<Key>, casemapped_equals_obj<Key, Key>>;

	/** Determines if \p str1 and \p str2 are equivalent when compared case insensitively using the
	 * configured IRC casemapping.
	 * @param str1 The first string to compare.
	 * @param str2 The second string to compare.
	 * @return True if \p str1 and \p str2 are equivalent; otherwise, false.
	 */
	CoreExport bool casemapped_equals(const std::string_view& str1, const std::string_view& str2);

	/** Determines if \p str1 is lexographically less than \p str2 when compared insensitively using
	 * the configured IRC casemapping.
	 * @param str1 The first string to compare.
	 * @param str2 The second string to compare.
	 * @return True if \p str1 is ranked lower than \p str2; otherwise, false.
	 */
	CoreExport bool casemapped_less(const std::string_view& str1, const std::string_view& str2);

	/** Determines the unique hash of \p str for use in a hash map when hashed insensitively using
	 * the configured IRC casemapping.
	 * @param str The string to hash.
	 * @return The unique hash of \p str.
	 */
	CoreExport size_t casemapped_hash(const std::string_view& str);
}

/** A casemapping that uses ASCII rules, i.e. A-Z are mapped to a-z. */
extern CoreExport const insp::casemap ascii_case_insensitive_map[256];

/** The currently configured casemapping. This defaults to \ref ascii_case_insensitive_map. */
extern CoreExport const insp::casemap* national_case_insensitive_map;

/** IRC casemapping variant of \p std::equal_to<String>. */
template<typename String1, typename String2>
struct insp::casemapped_equals_obj
{
	/** @copydoc insp::casemapped_equals */
	inline bool operator()(const String1& str1, const String2& str2) const
	{
		return insp::casemapped_equals(str1, str2);
	}
};

/** IRC casemapping variant of std::hash<String>. */
template<typename String>
struct insp::casemapped_hash_obj
{
	/** @copydoc insp::casemapped_hash */
	inline size_t operator()(const String& str) const
	{
		return insp::casemapped_hash(str);
	}
};

/** IRC casemapping variant of std::less<String>. */
template<typename String1, typename String2>
struct insp::casemapped_less_obj
{
	/** @copydoc insp::casemapped_less */
	inline bool operator()(const String1& str1, const String2& str2) const
	{
		return insp::casemapped_less(str1, str2);
	}
};
