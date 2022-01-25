/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Sadie Powell <sadie@witchery.services>
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
	class uncopiable;
}

/** Prevents instances of an inheriting type from being copied. */
class insp::uncopiable
{
private:
	/** Prevents copying via the copy constructor. */
	uncopiable(const uncopiable&) = delete;

	/** Prevents copying via the assignment operator. */
	uncopiable& operator=(const uncopiable&) = delete;

protected:
	/** Default constructor for the uncopiable class. */
	uncopiable() = default;

	/** Default destructor for the uncopiable class. */
	~uncopiable() = default;
};
