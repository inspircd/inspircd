/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

inline CrashState::CrashState(const char* Where, const void* Item)
	 : parent(ServerInstance->TraceData), where(Where), item(Item)
{
	ServerInstance->TraceData = this;
}
inline CrashState::~CrashState()
{
	ServerInstance->TraceData = parent;
}

