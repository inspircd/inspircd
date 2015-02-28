/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
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

class CapEvent
{
 public:
	enum CapEventType
	{
		CAPEVENT_REQ,
		CAPEVENT_LS,
		CAPEVENT_LIST,
		CAPEVENT_CLEAR
	};

	CapEventType type;
	std::vector<std::string> wanted;
	std::vector<std::string> ack;
	User* user;
	CapEvent(Module* sender, User* u, CapEventType capevtype) : type(capevtype), user(u) {}
};

class GenericCap : public Events::ModuleEventListener
{
	bool active;

 public:
	LocalIntExt ext;
	const std::string cap;
	GenericCap(Module* parent, const std::string& Cap)
		: Events::ModuleEventListener(parent, "event/cap")
		, active(true)
		, ext("cap_" + Cap, ExtensionItem::EXT_USER, parent)
		, cap(Cap)
	{
	}

	void OnCapEvent(CapEvent& ev)
	{
		if (!active)
			return;

		CapEvent *data = static_cast<CapEvent*>(&ev);
		if (data->type == CapEvent::CAPEVENT_REQ)
		{
			for (std::vector<std::string>::iterator it = data->wanted.begin(); it != data->wanted.end(); ++it)
			{
				if (it->empty())
					continue;
				bool enablecap = ((*it)[0] != '-');
				if (((enablecap) && (*it == cap)) || (*it == "-" + cap))
				{
					// we can handle this, so ACK it, and remove it from the wanted list
					data->ack.push_back(*it);
					data->wanted.erase(it);
					ext.set(data->user, enablecap ? 1 : 0);
					break;
				}
			}
		}
		else if (data->type == CapEvent::CAPEVENT_LS)
		{
			data->wanted.push_back(cap);
		}
		else if (data->type == CapEvent::CAPEVENT_LIST)
		{
			if (ext.get(data->user))
				data->wanted.push_back(cap);
		}
		else if (data->type == CapEvent::CAPEVENT_CLEAR)
		{
			data->ack.push_back("-" + cap);
			ext.set(data->user, 0);
		}
	}

	void SetActive(bool newstate) { active = newstate; }
	bool IsActive() const { return active; }
};
