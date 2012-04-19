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
	GenericCap(Module* parent, const std::string &Cap) : ext("cap_" + Cap, parent), cap(Cap)
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
