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


#ifndef SPANNINGTREE_H
#define SPANNINGTREE_H

struct AddServerEvent : public Event
{
	const std::string servername;
	AddServerEvent(Module* me, const std::string& name)
		: Event(me, "new_server"), servername(name)
	{
		Send();
	}
};

struct DelServerEvent : public Event
{
	const std::string servername;
	DelServerEvent(Module* me, const std::string& name)
		: Event(me, "lost_server"), servername(name)
	{
		Send();
	}
};

#endif
