/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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


#ifndef __CAP_H__
#define __CAP_H__

#include <map>
#include <string>

class CapData : public classbase
{
 public:
	irc::string type;
	std::vector<std::string> wanted;
	std::vector<std::string> ack;
	User* user;
	Module* creator;
};

void GenericCapHandler(Event* ev, const std::string &extname, const std::string &cap)
{
	if (ev->GetEventID() == "cap_req")
	{
		CapData *data = (CapData *) ev->GetData();

		std::vector<std::string>::iterator it;
		if ((it = std::find(data->wanted.begin(), data->wanted.end(), cap)) != data->wanted.end())
		{
			// we can handle this, so ACK it, and remove it from the wanted list
			data->ack.push_back(*it);
			data->wanted.erase(it);
			data->user->Extend(extname);
		}
	}

	if (ev->GetEventID() == "cap_ls")
	{
		CapData *data = (CapData *) ev->GetData();
		data->wanted.push_back(cap);
	}

	if (ev->GetEventID() == "cap_list")
	{
		CapData *data = (CapData *) ev->GetData();

		if (data->user->GetExt(extname))
			data->wanted.push_back(cap);
	}

	if (ev->GetEventID() == "cap_clear")
	{
		CapData *data = (CapData *) ev->GetData();
		data->ack.push_back("-" + cap);
		data->user->Shrink(extname);
	}
}

#endif
