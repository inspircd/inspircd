/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
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
	FakeUser(InspIRCd* Instance, const std::string &uid) : User(Instance, uid)
	{
		SetFd(FD_FAKEUSER_NUMBER);
	}

	virtual const std::string GetFullHost();
	virtual const std::string GetFullRealHost();
	void SetFakeServer(std::string name);
};

#endif
