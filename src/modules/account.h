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
	 * @param tag A hidden tag on the account, for recording freshness or login method
	 */
	virtual void DoLogin(User* user, const std::string& name, const std::string& tag = "") = 0;
};

class AccountDBEntry : public Extensible
{
 public:
	const irc::string name;
	const time_t ts;
	time_t hash_password_ts, connectclass_ts, tag_ts;
	std::string hash, password, connectclass, tag;
	AccountDBEntry(const irc::string& nameref, time_t ourTS) : Extensible(EXTENSIBLE_ACCOUNT), name(nameref), ts(ourTS), hash_password_ts(0), connectclass_ts(0), tag_ts(0), hash(""), password(""), connectclass(""), tag("")
	{
	}
};

typedef std::map<irc::string, AccountDBEntry*> AccountDB;

class AccountDBModifiedEvent : public Event
{
 public:
	const irc::string name;
	const AccountDBEntry* const entry;

	/**
	 * Create an event indicating that an account in the database has been modified
	 * The AccountDBProvider Send functions will automatically send this event
	 * @param me A pointer to the module creating the event
	 * @param acctname The name of the account that was modified
	 * @param ent A pointer to the account that was modified, or NULL if the account was deleted
	 */
	AccountDBModifiedEvent(Module* me, const irc::string& acctname, const AccountDBEntry* ent)
		: Event(me, "accountdb_modified"), name(acctname), entry(ent)
	{
	}
};

class AccountDBProvider : public DataProvider
{
 public:
	AccountDBProvider(Module* mod) : DataProvider(mod, "accountdb") {}

	/**
	 * Add an account to the database
	 * @param entry A pointer to the account to add
	 * @param send Whether or not to send the account immediately after adding it, if adding was successful
	 * @return True if the account was added, false if an account with the same name already existed
	 */
	virtual bool AddAccount(const AccountDBEntry* entry, bool send) = 0;

	/**
	 * Get an account from the database
	 * @param name The name of the account
	 * @return A pointer to the account, or NULL if no account by the given name exists
	 */
	virtual AccountDBEntry* GetAccount(irc::string name) = 0;

	/**
	 * Remove an account from the database
	 * Note that this does not free the memory belonging to the account
	 * @param entry A pointer to the account to remove
	 * @param send Whether or not to send the removal immediately after removing it
	 */
	virtual void RemoveAccount(const AccountDBEntry* entry, bool send) = 0;

	/**
	 * Get the internal database used to store accounts
	 * @return A const reference to the database
	 */
	virtual const AccountDB& GetDB() = 0;

	/**
	 * Send an entire account
	 * @param entry A pointer to the account to send
	 */
	virtual void SendAccount(const AccountDBEntry* entry) = 0;

	/** Send an update to an account
	 * @param entry A pointer to the account to send an update for
	 * @param field The name of the field to send an update for
	 */
	virtual void SendUpdate(const AccountDBEntry* entry, std::string field) = 0;

	/** Send a removal for an account
	 * Note that this does not free the memory belonging to the account
	 * @param entry A pointer to the account to send a removal for
	 */
	virtual void SendRemoval(const AccountDBEntry* entry) = 0;

	/** Send a removal for an account
	 * @param name The name of the account to remove
	 * @param ts The creation TS of the account to remove
	 */
	virtual void SendRemoval(irc::string name, time_t ts) = 0;
};

#endif
