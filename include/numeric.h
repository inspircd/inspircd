/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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

#include "numerics.h"

namespace Numeric
{
	class Numeric;
}

class Numeric::Numeric
{
	/** Numeric number
	 */
	unsigned int numeric;

	/** Parameters of the numeric
	 */
	std::vector<std::string> params;

 public:
	/** Constructor
	 * @param num Numeric number (RPL_*, ERR_*)
	 */
	Numeric(unsigned int num)
		: numeric(num)
	{
	}

	/** Add a parameter to the numeric. The parameter will be converted to a string first with ConvToStr().
	 * @param x Parameter to add
	 */
	template <typename T>
	Numeric& push(const T& x)
	{
		params.push_back(ConvToStr(x));
		return *this;
	}

	/** Get the number of the numeric as an unsigned integer
	 * @return Numeric number as an unsigned integer
	 */
	unsigned int GetNumeric() const { return numeric; }

	/** Get the parameters of the numeric
	 * @return Parameters of the numeric as a const vector of strings
	 */
	const std::vector<std::string>& GetParams() const { return params; }

	/** Get the parameters of the numeric
	 * @return Parameters of the numeric as a vector of strings
	 */
	std::vector<std::string>& GetParams() { return params; }
};

namespace Numerics
{
	/** ERR_NOSUCHNICK numeric
	 */
	class NoSuchNick : public Numeric::Numeric
	{
	 public:
		NoSuchNick(const std::string& nick)
			: Numeric(ERR_NOSUCHNICK)
		{
			push(nick);
			push("No such nick/channel");
		}
	};
}
