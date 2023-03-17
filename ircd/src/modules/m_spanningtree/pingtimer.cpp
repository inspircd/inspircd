/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2015 Attila Molnar <attilamolnar@hush.com>
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

#include "pingtimer.h"
#include "treeserver.h"
#include "commandbuilder.h"

PingTimer::PingTimer(TreeServer* ts)
    : Timer(Utils->PingFreq)
    , server(ts)
    , state(PS_SENDPING) {
}

PingTimer::State PingTimer::TickInternal() {
    // Timer expired, take next action based on what happened last time
    if (state == PS_SENDPING) {
        // Last ping was answered, send next ping
        server->GetSocket()->WriteLine(CmdBuilder("PING").push(server->GetId()));
        LastPingMsec = ServerInstance->Time() * 1000 + (ServerInstance->Time_ns() /
                       1000000);
        // Warn next unless warnings are disabled. If they are, jump straight to timeout.
        if (Utils->PingWarnTime) {
            return PS_WARN;
        } else {
            return PS_TIMEOUT;
        }
    } else if (state == PS_WARN) {
        // No pong arrived in PingWarnTime seconds, send a warning to opers
        ServerInstance->SNO->WriteToSnoMask('l',
                                            "Server \002%s\002 has not responded to PING for %d seconds, high latency.",
                                            server->GetName().c_str(), GetInterval());
        return PS_TIMEOUT;
    } else { // PS_TIMEOUT
        // They didn't answer the last ping, if they are locally connected, get rid of them
        if (server->IsLocal()) {
            TreeSocket* sock = server->GetSocket();
            sock->SendError("Ping timeout");
            sock->Close();
        }

        // If the server is non-locally connected, don't do anything until we get a PONG.
        // This is to avoid pinging the server and warning opers more than once.
        // If they do answer eventually, we will move to the PS_SENDPING state and ping them again.
        return PS_IDLE;
    }
}

void PingTimer::SetState(State newstate) {
    state = newstate;

    // Set when should the next Tick() happen based on the state
    if (state == PS_SENDPING) {
        SetInterval(Utils->PingFreq);
    } else if (state == PS_WARN) {
        SetInterval(Utils->PingWarnTime);
    } else if (state == PS_TIMEOUT) {
        SetInterval(Utils->PingFreq - Utils->PingWarnTime);
    }

    // If state == PS_IDLE, do not set the timer, see above why
}

bool PingTimer::Tick(time_t currtime) {
    if (server->IsDead()) {
        return false;
    }

    SetState(TickInternal());
    return false;
}

void PingTimer::OnPong() {
    // Calculate RTT
    long ts = ServerInstance->Time() * 1000 + (ServerInstance->Time_ns() / 1000000);
    server->rtt = ts - LastPingMsec;

    // Change state to send ping next, also reschedules the timer appropriately
    SetState(PS_SENDPING);
}
