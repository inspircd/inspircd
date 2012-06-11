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


#ifndef M_SPANNINGTREE_RESOLVERS_H
#define M_SPANNINGTREE_RESOLVERS_H

#include "socket.h"
#include "inspircd.h"
#include "xline.h"

#include "utils.h"
#include "link.h"

/** Handle resolving of server IPs for the cache
 */
class SecurityIPResolver : public Resolver
{
 private:
	reference<Link> MyLink;
	SpanningTreeUtilities* Utils;
	Module* mine;
	std::string host;
	QueryType query;
 public:
	SecurityIPResolver(Module* me, SpanningTreeUtilities* U, const std::string &hostname, Link* x, bool &cached, QueryType qt);
	void OnLookupComplete(const std::string &result, unsigned int ttl, bool cached);
	void OnError(ResolverError e, const std::string &errormessage);
};

/** This class is used to resolve server hostnames during /connect and autoconnect.
 * As of 1.1, the resolver system is seperated out from BufferedSocket, so we must do this
 * resolver step first ourselves if we need it. This is totally nonblocking, and will
 * callback to OnLookupComplete or OnError when completed. Once it has completed we
 * will have an IP address which we can then use to continue our connection.
 */
class ServernameResolver : public Resolver
{
 private:
	SpanningTreeUtilities* Utils;
	QueryType query;
	std::string host;
	reference<Link> MyLink;
	reference<Autoconnect> myautoconnect;
 public:
	ServernameResolver(SpanningTreeUtilities* Util, const std::string &hostname, Link* x, bool &cached, QueryType qt, Autoconnect* myac);
	void OnLookupComplete(const std::string &result, unsigned int ttl, bool cached);
	void OnError(ResolverError e, const std::string &errormessage);
};

#endif
