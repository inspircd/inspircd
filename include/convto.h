/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019, 2021, 2023-2024 Sadie Powell <sadie@witchery.services>
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
template<typename Stringable> requires(std::integral<Stringable> || std::floating_point<Stringable>)
inline std::string ConvToStr(const Stringable& in)
{
	return std::to_string(in);
}

/** Converts a type that fmtlib can format to a string.
 * @param in The value to convert.
 */
template<typename Formattable> requires(!std::integral<Formattable> && !std::floating_point<Formattable> && fmt_formattable<Formattable>)
inline std::string ConvToStr(const Formattable& in)
{
	return FMT::format("{}", in);
}

/** Converts a string to a numeric type.
 * @param in The string to convert to a numeric type.
 * @param def The value to return if the string could not be converted (defaults to 0)
 */
template<typename Numeric>
inline Numeric ConvToNum(const std::string& in, Numeric def = 0)
{
	Numeric ret;
	if (std::from_chars(in.data(), in.data() + in.size(), ret).ec != std::errc{})
		return def;
	return ret;
}
