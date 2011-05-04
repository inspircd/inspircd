/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __SPANNINGTREE_H__
#define __SPANNINGTREE_H__

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
