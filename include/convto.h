/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019, 2021, 2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2016 Attila Molnar <attilamolnar@hush.com>
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

/** No-op function that returns the string that was passed to it.
 * @param in The string to return.
 */
inline const std::string& ConvToStr(const std::string& in)
{
	return in;
}

/** Converts a string_view to a string.
 * @param in The value to convert.
 */
inline std::string ConvToStr(const std::string_view& in)
{
	return std::string(in);
}

/** Converts a char array to a string.
 * @param in The value to convert.
 */
inline std::string ConvToStr(const char* in)
{
	return std::string(in);
}

/** Converts a char to a string.
 * @param in The value to convert.
 */
inline std::string ConvToStr(char in)
{
	return std::string(1, static_cast<std::string::value_type>(in));
}

/** Converts an unsigned char to a string.
 * @param in The value to convert.
 */
inline std::string ConvToStr(unsigned char in)
{
	return std::string(1, static_cast<std::string::value_type>(in));
}

/** Converts a bool to a string.
 * @param in The value to convert.
 */
inline std::string ConvToStr(const bool in)
{
	return (in ? "1" : "0");
}

/** Converts a type that to_string is implemented for to a string.
 * @param in The value to convert.
 */
template<typename Stringable>
inline std::enable_if_t<std::is_arithmetic_v<Stringable>, std::string> ConvToStr(const Stringable& in)
{
	return std::to_string(in);
}

/** Converts any type to a string.
 * @param in The value to convert.
 */
template <class T>
inline std::enable_if_t<!std::is_arithmetic_v<T>, std::string> ConvToStr(const T& in)
{
	std::stringstream tmp;
	if (!(tmp << in))
		return std::string();
	return tmp.str();
}

/** Converts a string to a numeric type.
 * @param in The string to convert to a numeric type.
 * @param def The value to return if the string could not be converted (defaults to 0)
 */
template<typename Numeric>
inline Numeric ConvToNum(const std::string& in, Numeric def = 0)
{
	Numeric ret;
	std::istringstream tmp(in);
	if (!(tmp >> ret))
		return def;
	return ret;
}

/** Specialisation of ConvToNum so istringstream doesn't try to extract a text character.
 * @param in The string to convert to a numeric type.
 * @param def The value to return if the string could not be converted (defaults to 0)
 */
template<>
inline char ConvToNum<char>(const std::string& in, char def)
{
	int16_t num = ConvToNum<int16_t>(in, def);
	return num >= INT8_MIN && num <= INT8_MAX ? static_cast<char>(num) : def;
}

/** Specialisation of ConvToNum so istringstream doesn't try to extract a text character.
 * @param in The string to convert to a numeric type.
 * @param def The value to return if the string could not be converted (defaults to 0)
 */
template<>
inline unsigned char ConvToNum<unsigned char>(const std::string& in, unsigned char def)
{
	uint16_t num = ConvToNum<uint16_t>(in, def);
	return num <= UINT8_MAX ? static_cast<unsigned char>(num) : def;
}
