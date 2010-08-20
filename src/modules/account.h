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

#ifndef __ACCOUNT_H__
#define __ACCOUNT_H__

class AccountEvent : public Event
{
 public:
	User* const user;
	const std::string account;
	AccountEvent(Module* me, User* u, const std::string& name)
		: Event(me, "account_login"), user(u), account(name)
	{
	}
};

/** AccountProvider: use dynamic_reference<AccountProvider> acct("account") to access. */
class AccountProvider : public DataProvider
{
 public:
	AccountProvider(Module* mod, const std::string& Name) : DataProvider(mod, Name) {}
	/** Is the user registered? */
	virtual bool IsRegistered(User* user) = 0;
	/**
	 * Get the account name that a user is using
	 * @param user The user to query
	 * @return The account name, or "" if not logged in
	 */
	virtual std::string GetAccountName(User* user) = 0;
	/**
	 * Log the user in to an account.
	 *
	 * @param user The user to log in
	 * @param name The account name to log them in with. Empty to log out.
	 */
	virtual void DoLogin(User* user, const std::string& name) = 0;
};

#endif
