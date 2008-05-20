/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __CHANNELMANAGER_H
#define __CHANNELMANAGER_H

class CoreExport ChannelManager : public Extensible
{
 private:
	InspIRCd *ServerInstance;
 public:
	ChannelManager(InspIRCd *Instance) : ServerInstance(Instance)
	{
	}
};

#endif
