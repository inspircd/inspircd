/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
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

class CoreExport Server : public classbase
{
 protected:
	/** The unique identifier for this server. */
	const std::string id;

	/** The name of this server
	 */
	const std::string name;

	/** The description of this server.
	 * This can be updated by the protocol module (for remote servers) or by a rehash (for the local server).
	 */
	std::string description;

	/** True if this server is ulined
	 */
	bool uline;

	/** True if this server is a silent uline, i.e. silent="yes" in the uline block
	 */
	bool silentuline;

	/** Allow ConfigReaderThread to update the description on a rehash
	 */
	friend class ConfigReaderThread;

 public:
	Server(const std::string& srvid, const std::string& srvname, const std::string& srvdesc)
		: id(srvid)
		, name(srvname)
		, description(srvdesc)
		, uline(false)
		, silentuline(false)
	{
	}

	DEPRECATED_METHOD(Server(const std::string& srvname, const std::string& srvdesc))
		: name(srvname)
		, description(srvdesc)
		, uline(false)
		, silentuline(false)
	{
	}

	/** Retrieves the unique identifier for this server (e.g. 36C). */
	const std::string& GetId() const { return id; }

	/**
	 * Returns the name of this server
	 * @return The name of this server, for example "irc.example.com".
	 */
	const std::string& GetName() const { return name; }

	/** Returns the public name of this server respecting <security:hideserver> if set. */
	const std::string& GetPublicName() const;

	/** Returns the description of this server
	 * @return The description of this server
	 */
	const std::string& GetDesc() const { return description; }

	/**
	 * Checks whether this server is ulined
	 * @return True if this server is ulined, false otherwise.
	 */
	bool IsULine() const { return uline; }

	/**
	 * Checks whether this server is a silent uline
	 * Silent uline servers introduce, quit and oper up users without a snotice being generated.
	 * @return True if this server is a silent uline, false otherwise.
	 */
	bool IsSilentULine() const { return silentuline; }
};
