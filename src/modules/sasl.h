/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __SASL_H__
#define __SASL_H__

class SASLHook : public classbase
{
 public:
	SASLHook() {}
	virtual ~SASLHook() {}
	virtual void ProcessClient(User* user, const std::vector<std::string>& parameters) = 0;
	virtual void ProcessServer(User* user, const std::vector<std::string>& parameters) = 0;
};

class SASLSearch : public Event
{
 public:
	User* const user;
	const std::string method;
	SASLHook* auth;
	SASLSearch(Module* me, User* u, const std::string& m)
		: Event(me, "sasl_search"), user(u), method(m), auth(NULL)
	{
		Send();
	}
};

#endif
