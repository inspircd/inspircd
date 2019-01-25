/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
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

/** Handle resolving of server IPs for the cache
 */
class SecurityIPResolver : public DNS::Request
{
 private:
	reference<Link> MyLink;
	Module* mine;
	std::string host;
	DNS::QueryType query;
 public:
	SecurityIPResolver(Module* me, DNS::Manager* mgr, const std::string& hostname, Link* x, DNS::QueryType qt);
	void OnLookupComplete(const DNS::Query *r) override;
	void OnError(const DNS::Query *q) override;
};

/** This class is used to resolve server hostnames during /connect and autoconnect.
 * As of 1.1, the resolver system is seperated out from BufferedSocket, so we must do this
 * resolver step first ourselves if we need it. This is totally nonblocking, and will
 * callback to OnLookupComplete or OnError when completed. Once it has completed we
 * will have an IP address which we can then use to continue our connection.
 */
class ServernameResolver : public DNS::Request
{
 private:
	DNS::QueryType query;
	std::string host;
	reference<Link> MyLink;
	reference<Autoconnect> myautoconnect;
 public:
	ServernameResolver(DNS::Manager* mgr, const std::string& hostname, Link* x, DNS::QueryType qt, Autoconnect* myac);
	void OnLookupComplete(const DNS::Query *r) override;
	void OnError(const DNS::Query *q) override;
};
