#ifndef __HANDSHAKE_TIMER_H__
#define __HANDSHAKE_TIMER_H__

#include "inspircd.h"
#include "timer.h"

class SpanningTreeUtilities;
class TreeSocket;
class Link;

class HandshakeTimer : public InspTimer
{
 private:
	InspIRCd* Instance;
	TreeSocket* sock;
	Link* lnk;
	SpanningTreeUtilities* Utils;
	int thefd;
 public:
	HandshakeTimer(InspIRCd* Inst, TreeSocket* s, Link* l, SpanningTreeUtilities* u);
	virtual void Tick(time_t TIME);
};

#endif
