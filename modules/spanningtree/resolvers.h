/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2019, 2021-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <brain@inspircd.org>
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

#include "inspircd.h"
#include "modules/dns.h"

#include "utils.h"
#include "link.h"

// Handles resolving whitelisted hostnames for the inbound connection whitelist.
class SecurityIPResolver final
	: public DNS::Request
{
private:
	std::shared_ptr<Link> link;
	bool CheckIPv4();

public:
	SecurityIPResolver(Module* mod, DNS::Manager* mgr, const std::string& hostname, const std::shared_ptr<Link>& l, DNS::QueryType qt);
	void OnLookupComplete(const DNS::Query* r) override;
	void OnError(const DNS::Query* q) override;
};

// Handles resolving server hostnames when making an outbound connection.
class ServerNameResolver final
	: public DNS::Request
{
private:
	std::shared_ptr<Autoconnect> autoconnect;
	std::shared_ptr<Link> link;

public:
	ServerNameResolver(DNS::Manager* mgr, const std::string& hostname, const std::shared_ptr<Link>& l, DNS::QueryType qt, const std::shared_ptr<Autoconnect>& a);
	void OnLookupComplete(const DNS::Query* r) override;
	void OnError(const DNS::Query* q) override;
};
