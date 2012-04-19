/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "socket.h"
#include "xline.h"

#include "resolvers.h"
#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "link.h"
#include "treesocket.h"

/* $ModDep: m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h */

/** This class is used to resolve server hostnames during /connect and autoconnect.
 * As of 1.1, the resolver system is seperated out from BufferedSocket, so we must do this
 * resolver step first ourselves if we need it. This is totally nonblocking, and will
 * callback to OnLookupComplete or OnError when completed. Once it has completed we
 * will have an IP address which we can then use to continue our connection.
 */
ServernameResolver::ServernameResolver(SpanningTreeUtilities* Util, const std::string &hostname, Link* x, bool &cached, QueryType qt, Autoconnect* myac)
	: Resolver(hostname, qt, cached, Util->Creator), Utils(Util), query(qt), host(hostname), MyLink(x), myautoconnect(myac)
{
}

void ServernameResolver::OnLookupComplete(const std::string &result, unsigned int ttl, bool cached)
{
	/* Initiate the connection, now that we have an IP to use.
	 * Passing a hostname directly to BufferedSocket causes it to
	 * just bail and set its FD to -1.
	 */
	TreeServer* CheckDupe = Utils->FindServer(MyLink->Name.c_str());
	if (!CheckDupe) /* Check that nobody tried to connect it successfully while we were resolving */
	{
		TreeSocket* newsocket = new TreeSocket(Utils, MyLink, myautoconnect, result);
		if (newsocket->GetFd() > -1)
		{
			/* We're all OK */
		}
		else
		{
			/* Something barfed, show the opers */
			ServerInstance->SNO->WriteToSnoMask('l', "CONNECT: Error connecting \002%s\002: %s.",
				MyLink->Name.c_str(), newsocket->getError().c_str());
			ServerInstance->GlobalCulls.AddItem(newsocket);
		}
	}
}

void ServernameResolver::OnError(ResolverError e, const std::string &errormessage)
{
	/* Ooops! */
	if (query == DNS_QUERY_AAAA)
	{
		bool cached;
		ServernameResolver* snr = new ServernameResolver(Utils, host, MyLink, cached, DNS_QUERY_A, myautoconnect);
		ServerInstance->AddResolver(snr, cached);
		return;
	}
	ServerInstance->SNO->WriteToSnoMask('l', "CONNECT: Error connecting \002%s\002: Unable to resolve hostname - %s", MyLink->Name.c_str(), errormessage.c_str() );
	Utils->Creator->ConnectServer(myautoconnect, false);
}

