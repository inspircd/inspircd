/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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
