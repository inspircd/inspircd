/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
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

/** Template function to convert any input type to std::string
 */
template<typename T> inline std::string ConvNumeric(const T& in)
{
	if (in == 0)
		return "0";
	T quotient = in;
	std::string out;
	while (quotient)
	{
		out += "0123456789"[std::abs((long)quotient % 10)];
		quotient /= 10;
	}
	if (in < 0)
		out += '-';
	std::reverse(out.begin(), out.end());
	return out;
}

/** Template function to convert any input type to std::string
 */
inline std::string ConvToStr(const int in)
{
	return ConvNumeric(in);
}

/** Template function to convert any input type to std::string
 */
inline std::string ConvToStr(const long in)
{
	return ConvNumeric(in);
}

/** Template function to convert any input type to std::string
 */
inline std::string ConvToStr(const char* in)
{
	return in;
}

/** Template function to convert any input type to std::string
 */
inline std::string ConvToStr(const bool in)
{
	return (in ? "1" : "0");
}

/** Template function to convert any input type to std::string
 */
inline std::string ConvToStr(char in)
{
	return std::string(1, in);
}

inline const std::string& ConvToStr(const std::string& in)
{
	return in;
}

/** Template function to convert any input type to std::string
 */
template <class T> inline std::string ConvToStr(const T& in)
{
	std::stringstream tmp;
	if (!(tmp << in))
		return std::string();
	return tmp.str();
}

/** Template function to convert a std::string to any numeric type.
 */
template<typename TOut> inline TOut ConvToNum(const std::string& in)
{
	TOut ret;
	std::istringstream tmp(in);
	if (!(tmp >> ret))
		return 0;
	return ret;
}
