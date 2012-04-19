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

#include <map>
#include <string>

class CapEvent : public Event
{
 public:
	irc::string type;
	std::vector<std::string> wanted;
	std::vector<std::string> ack;
	User* user;
	Module* creator;
	CapEvent(Module* sender, const std::string& t) : Event(sender, t) {}
};

class GenericCap
{
 public:
	LocalIntExt ext;
	const std::string cap;
	GenericCap(Module* parent, const std::string &Cap) : ext(EXTENSIBLE_USER, "cap_" + Cap, parent), cap(Cap)
	{
		ServerInstance->Extensions.Register(&ext);
	}

	void HandleEvent(Event& ev)
	{
		CapEvent *data = static_cast<CapEvent*>(&ev);
		if (ev.id == "cap_req")
		{
			std::vector<std::string>::iterator it;
			if ((it = std::find(data->wanted.begin(), data->wanted.end(), cap)) != data->wanted.end())
			{
				// we can handle this, so ACK it, and remove it from the wanted list
				data->ack.push_back(*it);
				data->wanted.erase(it);
				ext.set(data->user, 1);
			}
		}

		if (ev.id == "cap_ls")
		{
			data->wanted.push_back(cap);
		}

		if (ev.id == "cap_list")
		{
			if (ext.get(data->user))
				data->wanted.push_back(cap);
		}

		if (ev.id == "cap_clear")
		{
			data->ack.push_back("-" + cap);
			ext.set(data->user, 0);
		}
	}
};

#endif
