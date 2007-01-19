#include "configreader.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "inspircd.h"
#include "wildcard.h"
#include "xline.h"
#include "transport.h"

#include "m_spanningtree/timesynctimer.h"
#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/link.h"
#include "m_spanningtree/treesocket.h"

TimeSyncTimer::TimeSyncTimer(InspIRCd *Inst, ModuleSpanningTree *Mod) : InspTimer(43200, Inst->Time(), true), Instance(Inst), Module(Mod)
{
}

void TimeSyncTimer::Tick(time_t TIME)
{
	Module->BroadcastTimeSync();
}

