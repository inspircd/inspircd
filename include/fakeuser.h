/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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
