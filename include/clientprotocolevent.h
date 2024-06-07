/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018, 2021-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 Attila Molnar <attilamolnar@hush.com>
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

#include "clientprotocolmsg.h"

namespace ClientProtocol
{
	namespace Events
	{
		struct Join;
		class Mode;
	}
}

struct CoreExport ClientProtocol::Events::Join
	: public ClientProtocol::Messages::Join, public ClientProtocol::Event
{
	Join()
		: ClientProtocol::Event(ServerInstance->GetRFCEvents().join, *this)
	{
	}

	Join(Membership* Memb)
		: ClientProtocol::Messages::Join(Memb)
		, ClientProtocol::Event(ServerInstance->GetRFCEvents().join, *this)
	{
	}

	Join(Membership* Memb, const std::string& Sourcestr)
		: ClientProtocol::Messages::Join(Memb, Sourcestr)
		, ClientProtocol::Event(ServerInstance->GetRFCEvents().join, *this)
	{
	}
};

class CoreExport ClientProtocol::Events::Mode
	: public ClientProtocol::Event
{
	std::list<ClientProtocol::Messages::Mode> modelist;
	std::vector<Message*> modemsgplist;
	const Modes::ChangeList& modechanges;

public:
	static void BuildMessages(User* source, Channel* Chantarget, User* Usertarget, const Modes::ChangeList& changelist, std::list<ClientProtocol::Messages::Mode>& modelist, std::vector<Message*>& modemsgplist)
	{
		// Build as many MODEs as necessary
		for (Modes::ChangeList::List::const_iterator i = changelist.getlist().begin(); i != changelist.getlist().end(); i = modelist.back().GetEndIterator())
		{
			modelist.push_back(ClientProtocol::Messages::Mode(source, Chantarget, Usertarget, changelist, i));
			modemsgplist.push_back(&modelist.back());
		}
	}

	Mode(User* source, Channel* Chantarget, User* Usertarget, const Modes::ChangeList& changelist)
		: ClientProtocol::Event(ServerInstance->GetRFCEvents().mode)
		, modechanges(changelist)
	{
		BuildMessages(source, Chantarget, Usertarget, changelist, modelist, modemsgplist);
		SetMessageList(modemsgplist);
	}

	const Modes::ChangeList& GetChangeList() const { return modechanges; }
	const std::list<ClientProtocol::Messages::Mode>& GetMessages() const { return modelist; }
};
