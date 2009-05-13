/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __FAKEUSER_H__
#define __FAKEUSER_H__

#include "users.h"

class CoreExport FakeUser : public User
{
 public:
	FakeUser(InspIRCd* Instance) : User(Instance, "!")
	{
		SetFd(FD_MAGIC_NUMBER);
	}

	virtual const std::string GetFullHost() { return server; }
	virtual const std::string GetFullRealHost() { return server; }
};

#endif
