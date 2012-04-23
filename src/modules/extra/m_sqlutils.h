/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
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


#ifndef INSPIRCD_SQLUTILS
#define INSPIRCD_SQLUTILS

#include "modules.h"

#define SQLUTILAU "SQLutil AssociateUser"
#define SQLUTILAC "SQLutil AssociateChan"
#define SQLUTILUA "SQLutil UnAssociate"
#define SQLUTILGU "SQLutil GetAssocUser"
#define SQLUTILGC "SQLutil GetAssocChan"
#define SQLUTILSUCCESS "You shouldn't be reading this (success)"

/** Used to associate an SQL query with a user
 */
class AssociateUser : public Request
{
public:
	/** Query ID
	 */
	unsigned long id;
	/** User
	 */
	User* user;

	AssociateUser(Module* s, Module* d, unsigned long i, User* u)
	: Request(s, d, SQLUTILAU), id(i), user(u)
	{
	}

	AssociateUser& S()
	{
		Send();
		return *this;
	}
};

/** Used to associate an SQL query with a channel
 */
class AssociateChan : public Request
{
public:
	/** Query ID
	 */
	unsigned long id;
	/** Channel
	 */
	Channel* chan;

	AssociateChan(Module* s, Module* d, unsigned long i, Channel* u)
	: Request(s, d, SQLUTILAC), id(i), chan(u)
	{
	}

	AssociateChan& S()
	{
		Send();
		return *this;
	}
};

/** Unassociate a user or  class from an SQL query
 */
class UnAssociate : public Request
{
public:
	/** The query ID
	 */
	unsigned long id;

	UnAssociate(Module* s, Module* d, unsigned long i)
	: Request(s, d, SQLUTILUA), id(i)
	{
	}

	UnAssociate& S()
	{
		Send();
		return *this;
	}
};

/** Get the user associated with an SQL query ID
 */
class GetAssocUser : public Request
{
public:
	/** The query id
	 */
	unsigned long id;
	/** The user
	 */
	User* user;

	GetAssocUser(Module* s, Module* d, unsigned long i)
	: Request(s, d, SQLUTILGU), id(i), user(NULL)
	{
	}

	GetAssocUser& S()
	{
		Send();
		return *this;
	}
};

/** Get the channel associated with an SQL query ID
 */
class GetAssocChan : public Request
{
public:
	/** The query id
	 */
	unsigned long id;
	/** The channel
	 */
	Channel* chan;

	GetAssocChan(Module* s, Module* d, unsigned long i)
	: Request(s, d, SQLUTILGC), id(i), chan(NULL)
	{
	}

	GetAssocChan& S()
	{
		Send();
		return *this;
	}
};

#endif
