/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019, 2021, 2024 Sadie Powell <sadie@witchery.services>
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

class CoreExport Server
	: public Cullable
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

	/** True if this server is a service. */
	bool service = false;

	/** True if this server is a silent service, i.e. silent="yes" in the service block. */
	bool silentservice = false;

	/** Allow ConfigReaderThread to update the description on a rehash
	 */
	friend class ConfigReaderThread;

public:
	Server(const std::string& srvid, const std::string& srvname, const std::string& srvdesc)
		: id(srvid)
		, name(srvname)
		, description(srvdesc)
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
	 * Checks whether this server is a service.
	 * @return True if this server is a service, false otherwise.
	 */
	bool IsService() const { return service; }

	/**
	 * Checks whether this server is a silent service.
	 * Silent services servers introduce, quit and oper up users without a snotice being generated.
	 * @return True if this server is a silent service, false otherwise.
	 */
	bool IsSilentService() const { return silentservice; }

	/** Send metadata related to this server to the target server.
	 * @param key The name of the metadata (e.g. foo-bar).
	 * @param data The network representation of the metadata.
	 */
	virtual void SendMetadata(const std::string& key, const std::string& data) const;

	/** Send metadata related to an extensible to the target server.
	 * @param ext The extensible to send metadata for.
	 * @param key The name of the metadata (e.g. foo-bar).
	 * @param data The network representation of the metadata.
	 */
	virtual void SendMetadata(const Extensible* ext, const std::string& key, const std::string& data) const;
};
