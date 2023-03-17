/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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


#pragma once

class TreeServer;

/** Handles PINGing servers and killing them on timeout
 */
class PingTimer : public Timer {
    enum State {
        /** Send PING next */
        PS_SENDPING,
        /** Warn opers next */
        PS_WARN,
        /** Kill the server next due to ping timeout */
        PS_TIMEOUT,
        /** Do nothing */
        PS_IDLE
    };

    /** Server the timer is interacting with
     */
    TreeServer* const server;

    /** What to do when the timer ticks next
     */
    State state;

    /** Last ping time in milliseconds, used to calculate round trip time
     */
    unsigned long LastPingMsec;

    /** Update internal state and reschedule timer according to the new state
     * @param newstate State to change to
     */
    void SetState(State newstate);

    /** Process timer tick event
     * @return State to change to
     */
    State TickInternal();

    /** Called by the TimerManager when the timer expires
     * @param currtime Time now
     * @return Always false, we reschedule ourselves instead
     */
    bool Tick(time_t currtime) CXX11_OVERRIDE;

  public:
    /** Construct the timer. This doesn't schedule the timer.
     * @param server TreeServer to interact with
     */
    PingTimer(TreeServer* server);

    /** Register a PONG from the server
     */
    void OnPong();
};
