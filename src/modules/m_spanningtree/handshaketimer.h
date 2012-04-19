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
