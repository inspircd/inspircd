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


#ifndef M_CAP_H
#define M_CAP_H

class GenericCap;

class CapEvent : public Event
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
	std::vector<std::pair<GenericCap*, int> > changed; // HACK: clean this up before 2.2
	User* user;
	CapEvent(Module* sender, User* u, CapEventType capevtype) : Event(sender, "cap_request"), type(capevtype), user(u) {}
};

class GenericCap
{
 public:
	LocalIntExt ext;
	const std::string cap;
	GenericCap(Module* parent, const std::string &Cap) : ext("cap_" + Cap, parent), cap(Cap)
	{
		ServerInstance->Modules->AddService(ext);
	}

	void HandleEvent(Event& ev)
	{
		if (ev.id != "cap_request")
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
					data->changed.push_back(std::make_pair(this, ext.set(data->user, enablecap ? 1 : 0)));
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
};

#endif
