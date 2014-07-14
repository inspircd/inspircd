/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Peter Powell <petpow@saberuk.com>
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

/** This class manages the generation and transmission of ISUPPORT. */
class CoreExport ISupportManager
{
 private:
	/** The generated lines which are sent to clients. */
	std::vector<std::string> Lines;

 public:
	/** (Re)build the ISUPPORT vector. */
	void Build();

	/** Returns the std::vector of ISUPPORT lines. */
	const std::vector<std::string>& GetLines()
	{
		return this->Lines;
	}

	/** Send the 005 numerics (ISUPPORT) to a user. */
	void SendTo(LocalUser* user);
};
