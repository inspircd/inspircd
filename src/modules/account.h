/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __ACCOUNT_H__
#define __ACCOUNT_H__

#include <map>
#include <string>

class AccountData : public classbase
{
 public:
	User* user;
	std::string account;
};

#endif
