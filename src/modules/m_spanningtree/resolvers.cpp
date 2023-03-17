/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013, 2016 Adam <Adam@anope.org>
 *   Copyright (C) 2012-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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


#include "inspircd.h"

#include "cachetimer.h"
#include "resolvers.h"
#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "link.h"
#include "treesocket.h"

/** This class is used to resolve server hostnames during /connect and autoconnect.
 * As of 1.1, the resolver system is separated out from BufferedSocket, so we must do this
 * resolver step first ourselves if we need it. This is totally nonblocking, and will
 * callback to OnLookupComplete or OnError when completed. Once it has completed we
 * will have an IP address which we can then use to continue our connection.
 */
ServernameResolver::ServernameResolver(DNS::Manager* mgr,
                                       const std::string& hostname, Link* x, DNS::QueryType qt, Autoconnect* myac)
    : DNS::Request(mgr, Utils->Creator, hostname, qt)
    , query(qt), host(hostname), MyLink(x), myautoconnect(myac) {
}

void ServernameResolver::OnLookupComplete(const DNS::Query *r) {
    const DNS::ResourceRecord* const ans_record = r->FindAnswerOfType(
                this->question.type);
    if (!ans_record) {
        OnError(r);
        return;
    }

    irc::sockets::sockaddrs sa;
    if (!irc::sockets::aptosa(ans_record->rdata, MyLink->Port, sa)) {
        // We had a result but it wasn't a valid IPv4/IPv6.
        OnError(r);
        return;
    }

    /* Initiate the connection, now that we have an IP to use.
     * Passing a hostname directly to BufferedSocket causes it to
     * just bail and set its FD to -1.
     */
    TreeServer* CheckDupe = Utils->FindServer(MyLink->Name);
    if (!CheckDupe) { /* Check that nobody tried to connect it successfully while we were resolving */
        TreeSocket* newsocket = new TreeSocket(MyLink, myautoconnect, sa);
        if (!newsocket->HasFd()) {
            /* Something barfed, show the opers */
            ServerInstance->SNO->WriteToSnoMask('l',
                                                "CONNECT: Error connecting \002%s\002: %s.",
                                                MyLink->Name.c_str(), newsocket->getError().c_str());
            ServerInstance->GlobalCulls.AddItem(newsocket);
        }
    }
}

void ServernameResolver::OnError(const DNS::Query *r) {
    if (r->error == DNS::ERROR_UNLOADED) {
        // We're being unloaded, skip the snotice and ConnectServer() below to prevent autoconnect creating new sockets
        return;
    }

    if (query == DNS::QUERY_AAAA) {
        ServernameResolver* snr = new ServernameResolver(this->manager, host, MyLink,
                DNS::QUERY_A, myautoconnect);
        try {
            this->manager->Process(snr);
            return;
        } catch (DNS::Exception &) {
            delete snr;
        }
    }

    ServerInstance->SNO->WriteToSnoMask('l',
                                        "CONNECT: Error connecting \002%s\002: Unable to resolve hostname - %s",
                                        MyLink->Name.c_str(), this->manager->GetErrorStr(r->error).c_str());
    Utils->Creator->ConnectServer(myautoconnect, false);
}

SecurityIPResolver::SecurityIPResolver(Module* me, DNS::Manager* mgr,
                                       const std::string& hostname, Link* x, DNS::QueryType qt)
    : DNS::Request(mgr, me, hostname, qt)
    , MyLink(x), mine(me), host(hostname), query(qt) {
}

bool SecurityIPResolver::CheckIPv4() {
    // We only check IPv4 addresses if we have checked IPv6.
    if (query != DNS::QUERY_AAAA) {
        return false;
    }

    SecurityIPResolver* res = new SecurityIPResolver(mine, this->manager, host,
            MyLink, DNS::QUERY_A);
    try {
        this->manager->Process(res);
        return true;
    } catch (const DNS::Exception&) {
        delete res;
        return false;
    }
}

void SecurityIPResolver::OnLookupComplete(const DNS::Query *r) {
    for (std::vector<reference<Link> >::iterator i = Utils->LinkBlocks.begin();
            i != Utils->LinkBlocks.end(); ++i) {
        Link* L = *i;
        if (L->IPAddr == host) {
            for (std::vector<DNS::ResourceRecord>::const_iterator j = r->answers.begin();
                    j != r->answers.end(); ++j) {
                const DNS::ResourceRecord& ans_record = *j;
                if (ans_record.type != this->question.type) {
                    continue;
                }

                Utils->ValidIPs.push_back(ans_record.rdata);
                ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT,
                                          "Resolved '%s' as a valid IP address for link '%s'",
                                          ans_record.rdata.c_str(), MyLink->Name.c_str());
            }
            break;
        }
    }

    CheckIPv4();
}

void SecurityIPResolver::OnError(const DNS::Query *r) {
    // This can be called because of us being unloaded but we don't have to do anything differently
    if (CheckIPv4()) {
        return;
    }

    ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                              "Could not resolve IP associated with link '%s': %s",
                              MyLink->Name.c_str(), this->manager->GetErrorStr(r->error).c_str());
}

CacheRefreshTimer::CacheRefreshTimer()
    : Timer(3600, true) {
}

bool CacheRefreshTimer::Tick(time_t TIME) {
    Utils->RefreshIPCache();
    return true;
}
