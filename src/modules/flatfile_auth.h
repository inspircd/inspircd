/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __FLATFILE_AUTH_H__
#define __FLATFILE_AUTH_H__

class AccountDBEntry : public Extensible
{
 public:
	irc::string name;
	time_t ts, hash_password_ts, connectclass_ts, tag_ts;
	std::string hash, password, connectclass, tag;
	AccountDBEntry() : Extensible(EXTENSIBLE_ACCOUNT), name(""), ts(0), hash_password_ts(0), connectclass_ts(0), tag_ts(0), hash(""), password(""), connectclass(""), tag("") {}
};

typedef std::map<irc::string, AccountDBEntry*> AccountDB;

class AccountDBProvider : public DataProvider
{
 public:
	AccountDBProvider(Module* mod) : DataProvider(mod, "accountdb") {}
	virtual AccountDB& GetDB() = 0;
	virtual void SendUpdate(const AccountDBEntry* entry) = 0;
};

#endif
