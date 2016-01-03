/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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

class TestSuite;

class CoreExport UIDGenerator
{
#ifdef INSPIRCD_TEST
 public:
#endif

	/** Holds the current UID. Used to generate the next one.
	 */
	std::string current_uid;

	/** Increments the current UID by one.
	 */
	void IncrementUID(unsigned int pos);

 public:
	/**
	* This is the maximum length of a UUID (unique user identifier).
	* This length is set in compliance with TS6 protocol, and really should not be changed. Ever.
	* It allows for a lot of clients as-is. -- w00t.
	*/
	static const unsigned int UUID_LENGTH = 9;

	/** Initializes this UID generator with the given SID
	 * @param sid SID that conforms to InspIRCd::IsSID()
	 */
	void init(const std::string& sid);

	/** Returns the next available UID for this server.
	 */
	std::string GetUID();

	/** Generates a pseudorandom SID based on a servername and a description
	 * Guaranteed to return the same if invoked with the same parameters
	 * @param servername The server name to use as seed
	 * @param serverdesc The server description to use as seed
	 * @return A valid SID
	 */
	static std::string GenerateSID(const std::string& servername, const std::string& serverdesc);
};
