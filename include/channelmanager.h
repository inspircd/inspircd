/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __CHANNELMANAGER_H
#define __CHANNELMANAGER_H

/** THe channel manager class allocates and deallocates channels and manages
 * the container which holds them. For some reason, nobody finished this.
 * TODO: Finish in future release!
 */
class CoreExport ChannelManager : public Extensible
{
 private:
	InspIRCd *ServerInstance;
 public:
	/** Constructor
	 */
	ChannelManager(InspIRCd *Instance) : ServerInstance(Instance)
	{
	}
};

#endif
