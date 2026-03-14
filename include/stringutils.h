/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2021-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <brain@inspircd.org>
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

namespace Base64
{
	/** The default table used when handling Base64-encoded strings. */
	inline constexpr const char* TABLE = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

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
	inline std::string Decode(const std::string_view& data, const char* table = nullptr)
	{
		return Decode(data.data(), data.length(), table);
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
	inline std::string Encode(const std::string_view& data, const char* table = nullptr, char padding = 0)
	{
		return Encode(data.data(), data.length(), table, padding);
	}
}

namespace Hex
{
	/** The table used for encoding as a lower-case hexadecimal string. */
	inline constexpr const char* TABLE_LOWER = "0123456789abcdef";

	/** The table used for encoding as an upper-case hexadecimal string. */
	inline constexpr const char* TABLE_UPPER = "0123456789ABCDEF";

	/** Decodes a hexadecimal-encoded byte array.
	 * @param data The byte array to decode from.
	 * @param length The length of the byte array.
	 * @param separator If non-zero then the character hexadecimal digits are separated with.
	 * @param table The index table to use for decoding.
	 * @return The decoded form of the specified data.
	 */
	CoreExport std::string Decode(const void* data, size_t length, const char* table = nullptr, char separator = 0);

	/** Decodes a hexadecimal-encoded string.
	 * @param data The string to decode from.
	 * @param table The index table to use for decoding.
	 * @param separator If non-zero then the character hexadecimal digits are separated with.
	 * @return The decoded form of the specified data.
	 */
	inline std::string Decode(const std::string_view& data, const char* table = nullptr, char separator = 0)
	{
		return Decode(data.data(), data.length(), table, separator);
	}

	/** Encodes a byte array using hexadecimal encoding.
	 * @param data The byte array to encode from.
	 * @param length The length of the byte array.
	 * @param table The index table to use for encoding.
	 * @param separator If non-zero then the character to separate hexadecimal digits with.
	 * @return The encoded form of the specified data.
	 */
	CoreExport std::string Encode(const void* data, size_t length, const char* table = nullptr, char separator = 0);

	/** Encodes a string using Base64.
	 * @param data The string to encode from.
	 * @param table The index table to use for encoding.
	 * @param separator If non-zero then the character to separate hexadecimal digits with.
	 * @return The encoded form of the specified data.
	 */
	inline std::string Encode(const std::string_view& data, const char* table = nullptr, char separator = 0)
	{
		return Encode(data.data(), data.length(), table, separator);
	}
}

namespace Percent
{
	/** The table used to determine what characters are safe within a percent-encoded string. */
	inline constexpr const char* TABLE = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~";

	/** Decodes a percent-encoded byte array.
	 * @param data The byte array to decode from.
	 * @param length The length of the byte array.
	 * @return The decoded form of the specified data.
	 */
	CoreExport std::string Decode(const void* data, size_t length);

	/** Decodes a percent-encoded string.
	 * @param data The string to decode from.
	 * @return The decoded form of the specified data.
	 */
	inline std::string Decode(const std::string_view& data)
	{
		return Decode(data.data(), data.length());
	}

	/** Encodes a byte array using percent encoding.
	 * @param data The byte array to encode from.
	 * @param length The length of the byte array.
	 * @param table The table of characters that do not require escaping.
	 * @param upper Whether to use upper or lower case.
	 * @return The encoded form of the specified data.
	 */
	CoreExport std::string Encode(const void* data, size_t length, const char* table = nullptr, bool upper = true);

	/** Encodes a string using percent encoding.
	 * @param data The string to encode from.
	 * @param table The table of characters that do not require escaping.
	 * @param upper Whether to use upper or lower case.
	 * @return The encoded form of the specified data.
	 */
	inline std::string Encode(const std::string_view& data, const char* table = nullptr, bool upper = true)
	{
		return Encode(data.data(), data.length(), table, upper);
	}
}

namespace Template
{
	/** A mapping of variable names to their values. */
	typedef insp::flat_map<std::string, std::string> VariableMap;

	/** Replaces template variables like %foo% within a string.
	 * @param str The string to template from.
	 * @param vars The variables to replace within the string.
	 * @return The specified string with all variables replaced within it.
	 */
	CoreExport std::string Replace(const std::string_view& str, const VariableMap& vars);
}


/** Alows tokens in the IRC wire format to be read from a string. */
class CoreExport MessageTokenizer final
{
private:
	/** The message to parse tokens from. */
	std::string message;

	/** The current position in the message. */
	std::string::size_type position = 0;

public:
	/** Creates a message tokenizer and fills it with the provided data.
	 * @param msg The messsage to parse tokens from.
	 * @param start The index to start tokenizing from.
	 * @param end The index to stop tokenizing at.
	 */
	MessageTokenizer(const std::string& msg, std::string::size_type start = 0, std::string::size_type end = std::string::npos);

	/** Determines whether the tokenizer at at the end of the message. */
	inline auto AtEnd() const { return this->position >= this->message.length(); }

	/** Retrieves a reference to the whole message. */
	auto& GetMessage() { return this->message; }

	/** Retrieves the next \<middle> token in the message.
	 * @param token The next token available, or an empty string if none remain.
	 * @return True if a token was retrieved; otherwise, false.
	 */
	bool GetMiddle(std::string& token);

	/** Retrieves the next \<middle> token in the message.
	 * @param token The next token available, or an empty string view if none remain.
	 * @return True if a token was retrieved; otherwise, false.
	 */
	bool GetMiddle(std::string_view& token);

	/** Retrieves all remaining \<middle> tokens in the message.
	 * @param tokens A vector to place all remaining \<middle> tokens in.
	 */
	template<typename Value>
	void GetRemainingMiddle(std::vector<Value>& tokens)
	{
		tokens.clear();
		for (Value token; GetMiddle(token); )
			tokens.push_back(token);
	}

	/** Retrieves all remaining \<trailing> tokens in the message.
	 * @param tokens A vector to place all remaining \<trailing> tokens in.
	 */
	template<typename Value>
	void GetRemainingTrailing(std::vector<Value>& tokens)
	{
		tokens.clear();
		for (Value token; GetTrailing(token); )
			tokens.push_back(token);
	}

	/** Retrieves the current position in the message. */
	auto GetPosition() const { return this->position; }

	/** Retrieves the next \<trailing> token in the message.
	 * @param token The next token available, or an empty string if none remain.
	 * @return True if a token was retrieved; otherwise, false.
	 */
	bool GetTrailing(std::string& token);

	/** Retrieve the next \<trailing> token in the message.
	 * @param token The next token available, or an empty string view if none remain.
	 * @return True if a token was retrieved; otherwise, false.
	 */
	bool GetTrailing(std::string_view& token);
};

class CoreExport StringSplitter final
{
private:
	/** The current position in the string. */
	std::string::size_type position = 0;

public:
	/** Whether to allow empty values in the string. */
	const bool allow_empty;

	/** The value to split the string based on. */
	const std::string::value_type separator;

	/** The string to parse tokens from. */
	const std::string string;

	/** Creates a string splitter and fills it with the provided data.
	 * @param str The string to split.
	 * @param sep The value to split on.
	 * @param ae Whether to allow empty tokens within the string.
	 * @param start The index to start splitting from.
	 * @param end The index to stop splitting at.
	 */
	StringSplitter(const std::string& str, std::string::value_type sep = ' ', bool ae = false, std::string::size_type start = 0, std::string::size_type end = std::string::npos);

	/** Determines whether the splitter at at the end of the message. */
	inline auto AtEnd() const { return this->position >= this->string.length(); }

	/** Retrieves the current position in the string. */
	inline auto GetPosition() const { return this->position; }

	/** Retrieves a view to the part of the string which has not been split yet. */
	std::string_view GetRemaining() const;

	/** Retrieves the next token in the string.
	 * @param token The next token available, or an empty string if none remain.
	 * @return True if a token was retrieved; otherwise, false.
	 */
	bool GetToken(std::string& token);

	/** Retrieves the next token in the string.
	 * @param token The next token available, or an empty string view if none remain.
	 * @return True if a token was retrieved; otherwise, false.
	 */
	bool GetToken(std::string_view& token);

	/** Retrieves the next numeric token in the string.
	 * @param token The next token available, or an default-initialised value if none remain.
	 * @return True if a token was retrieved; otherwise, false.
	 */
	template<typename Numeric>
	bool GetToken(Numeric& token)
	{
		if (std::string_view sv; GetToken(sv))
		{
			token = ConvToNum<Numeric>(sv);
			return true;
		}

		token = Numeric();
		return true;
	}
};
