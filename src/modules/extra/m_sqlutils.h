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
