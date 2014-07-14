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
	std::vector<std::string> cachedlines;

 public:
	/** (Re)build the ISUPPORT vector.
	 * Called by the core on boot after all modules have been loaded, and every time when a module is loaded
	 * or unloaded. Calls the On005Numeric hook, letting modules manipulate the ISUPPORT tokens.
	 */
	void Build();

	/** Returns the cached std::vector of ISUPPORT lines.
	 * @return A list of strings prepared for sending to users
	 */
	const std::vector<std::string>& GetLines() const { return cachedlines; }

	/** Send the 005 numerics (ISUPPORT) to a user.
	 * @param user The user to send the ISUPPORT numerics to
	 */
	void SendTo(LocalUser* user);
};
