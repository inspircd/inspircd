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


#ifndef __HANDSHAKE_TIMER_H__
#define __HANDSHAKE_TIMER_H__

#include "inspircd.h"
#include "timer.h"

class SpanningTreeUtilities;
class TreeSocket;
class Link;

class HandshakeTimer : public Timer
{
 private:
	InspIRCd* Instance;
	TreeSocket* sock;
	Link* lnk;
	SpanningTreeUtilities* Utils;
	int thefd;
 public:
	HandshakeTimer(InspIRCd* Inst, TreeSocket* s, Link* l, SpanningTreeUtilities* u, int delay);
	~HandshakeTimer();
	virtual void Tick(time_t TIME);
};

#endif
