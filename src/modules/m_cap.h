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
