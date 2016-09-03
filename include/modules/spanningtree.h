/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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

#include "event.h"

class SpanningTreeEventListener : public Events::ModuleEventListener
{
 public:
	SpanningTreeEventListener(Module* mod)
		: ModuleEventListener(mod, "event/spanningtree")
	{
	}

	/** Fired when a server finishes burst
	 * @param server Server that recently linked and finished burst
	 */
	virtual void OnServerLink(const Server* server) { }

	 /** Fired when a server splits
	  * @param server Server that split
	  */
	virtual void OnServerSplit(const Server* server) { }
};
