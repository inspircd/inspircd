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
	const irc::string account;
	AccountEvent(Module* me, User* u, const irc::string& name)
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
	virtual irc::string GetAccountName(User* user) = 0;
	/**
	 * Log the user in to an account.
	 *
	 * @param user The user to log in
	 * @param name The account name to log them in with. Empty to log out.
	 * @param tag A hidden tag on the account, for recording freshness or login method
	 */
	virtual void DoLogin(User* user, const irc::string& name, const std::string& tag = "") = 0;
};

class AccountDBEntry : public Extensible
{
 public:
	const irc::string name;
	const time_t ts;
	time_t hash_password_ts;
	std::string hash, password;
	AccountDBEntry(const irc::string& nameref, time_t ourTS, std::string h = "", std::string p = "", time_t h_p_ts = 0, std::string cc = "", time_t cc_ts = 0) : Extensible(EXTENSIBLE_ACCOUNT), name(nameref), ts(ourTS), hash_password_ts(h_p_ts), hash(h), password(p)
	{
	}
	virtual CullResult cull() = 0;
	virtual ~AccountDBEntry() {}
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

class GetAccountByAliasEvent : public Event
{
 public:
	const irc::string account;
	AccountDBEntry* entry;
	time_t alias_ts;
	void (*RemoveAliasImpl)(const irc::string&);
	GetAccountByAliasEvent(Module* me, const irc::string& name)
		: Event(me, "get_account_by_alias"), account(name), entry(NULL), alias_ts(0), RemoveAliasImpl(NULL)
	{
		Send();
	}
	inline void RemoveAlias()
	{
		RemoveAliasImpl(account);
	}
};

class AccountDBProvider : public DataProvider
{
 public:
	AccountDBProvider(Module* mod) : DataProvider(mod, "accountdb") {}

	/**
	 * Create an account and add it to the database
	 * @param send Whether or not to send the account immediately after adding it, if adding was successful
	 * @param nameref The name of the account to add
	 * @param ourTS The creation TS of the account to add
	 * @param h The hash type of the account to add
	 * @param p The password of the account to add
	 * @param h_p_ts The hash/password TS of the account to add
	 * @param cc The connect class of the account to add
	 * @param cc_ts The TS of the connect class of the account to add
	 * @return A pointer to the new account if it was successfully added, NULL if an account with the same name already existed
	 */
	virtual AccountDBEntry* AddAccount(bool send, const irc::string& nameref, time_t ourTS, std::string h = "", std::string p = "", time_t h_p_ts = 0, std::string cc = "", time_t cc_ts = 0) = 0;

	/**
	 * Get an account from the database
	 * @param name The name of the account
	 * @param alias Whether or not to check the given name as an alias
	 * @return A pointer to the account, or NULL if no account by the given name exists
	 */
	virtual AccountDBEntry* GetAccount(irc::string name, bool alias) const = 0;

	/**
	 * Remove an account from the database and delete it
	 * This frees the memory associated with the account and invalidates any pointers to it
	 * @param send Whether or not to send the removal immediately after removing it
	 * @param entry A pointer to the account to remove
	 */
	virtual void RemoveAccount(bool send, AccountDBEntry* entry) = 0;

	/**
	 * Get the internal database used to store accounts
	 * @return A const reference to the database
	 */
	virtual const AccountDB& GetDB() const = 0;

	/**
	 * Send an entire account
	 * @param entry A pointer to the account to send
	 */
	virtual void SendAccount(const AccountDBEntry* entry) const = 0;

	/** Send an update to an account
	 * @param entry A pointer to the account to send an update for
	 * @param field The name of the field to send an update for
	 */
	virtual void SendUpdate(const AccountDBEntry* entry, std::string field) const = 0;

	/** Send a removal for an account
	 * @param name The name of the account to remove
	 * @param ts The creation TS of the account to remove
	 */
	virtual void SendRemoval(irc::string name, time_t ts) const = 0;
};

// Some generic extension items for use with accounts

class TSExtItem : public SimpleExtItem<time_t>
{
 public:
	TSExtItem(const std::string& Key, Module* parent) : SimpleExtItem<time_t>(EXTENSIBLE_ACCOUNT, Key, parent) {}
	std::string serialize(SerializeFormat format, const Extensible* container, void* item) const
	{
		time_t* ts = static_cast<time_t*>(item);
		if(format == FORMAT_USER)
		{
			if(!ts || !*ts)
				return "never";
			return ServerInstance->TimeString(*ts);
		}
		if(!ts)
			return "";
		return ConvToStr(*ts);
	}

	void unserialize(SerializeFormat format, Extensible* container, const std::string& value)
	{
		time_t* ours = get(container);
		time_t theirs = atol(value.c_str());
		if(!ours || theirs > *ours)
			set(container, theirs);
	}
};

template<typename T>
class TSGenericExtItem : public ExtensionItem
{
 protected:
	const T* const default_value;

	/**
	 * Serialize the value of the extension item
	 * @param format The format to serialize the value in
	 * @param value The value to serialize.  It it the caller's responsibility to make sure that it is not null.
	 * @return The serialized value
	 */
	virtual std::string value_serialize(SerializeFormat format, const T* value) const = 0;

	/**
	 * Unserialize the value of an extension item
	 * This function needs to allocate memory for the unserialized value.
	 * Its lifetime will be handled by the caller.
	 * @param format The format to unserialize the value from
	 * @param value The serialized value
	 * @return A pointer to the unserialized value.  This should not be null.
	 */
	virtual T* value_unserialize(SerializeFormat format, const std::string& value) = 0;

	/**
	 * Resolve a conflict when timestamps are identical
	 * @param value The existing value, which will be modified if necessary to resolve the conflict.  It is the caller's responsibility to make sure that it is not null.
	 * @param newvalue The new value.  It is the caller's responsibility to make sure that it is not null.
	 */
	virtual void value_resolve_conflict(T* value, const T* newvalue) = 0;

 public:
	typedef std::pair<time_t, T* const> value_pair;

	TSGenericExtItem(const std::string& Key, const T& default_val, Module* parent) : ExtensionItem(EXTENSIBLE_ACCOUNT, Key, parent), default_value(new T(default_val))
	{
	}

	TSGenericExtItem(const std::string& Key, T* default_val, Module* parent) : ExtensionItem(EXTENSIBLE_ACCOUNT, Key, parent), default_value(default_val)
	{
	}

	virtual ~TSGenericExtItem()
	{
		delete default_value;
	}

	inline value_pair* get(const Extensible* container) const
	{
		return static_cast<value_pair*>(get_raw(container));
	}

	inline T* get_value(const Extensible* container) const
	{
		value_pair* ptr = get(container);
		return ptr ? ptr->second : NULL;
	}

	inline void set(Extensible* container, time_t ts, const T& value)
	{
		value_pair* old = static_cast<value_pair*>(set_raw(container, new value_pair(ts, new T(value))));
		if(old)
		{
			delete old->second;
			delete old;
		}
	}

	inline void set(Extensible* container, time_t ts, T* value)
	{
		value_pair* old = static_cast<value_pair*>(set_raw(container, new value_pair(ts, value)));
		if(old)
		{
			delete old->second;
			delete old;
		}
	}

	inline void set(Extensible* container, const T& value)
	{
		set(container, ServerInstance->Time(), value);
	}

	inline void set(Extensible* container, T* value)
	{
		set(container, ServerInstance->Time(), value);
	}

	// The way account extension items are synchronized between servers, it would always be a bug to unset one, so the lack of an unset function is deliberate.

	virtual std::string serialize(SerializeFormat format, const Extensible* container, void* item) const
	{
		value_pair* p = static_cast<value_pair*>(item);
		if(!p)
		{
			if(format == FORMAT_USER)
				return value_serialize(FORMAT_USER, default_value);
			return "";
		}
		if(format == FORMAT_USER)
			return value_serialize(FORMAT_USER, p->second);
		return ConvToStr(p->first) + (format == FORMAT_NETWORK ? " :" : " ") + value_serialize(format, p->second);
	}

	virtual void unserialize(SerializeFormat format, Extensible* container, const std::string& value)
	{
		time_t ts;
		T* ptr;
		std::string::size_type delim = value.find_first_of(' ');
		ts = atol(value.substr(0, delim).c_str());
		if(delim == std::string::npos)
			ptr = new T(*default_value);
		else
			ptr = value_unserialize(format, value.substr(delim + 1));
		value_pair* p = get(container);
		if(!p || ts > p->first)
			set(container, ts, ptr);
		else
		{
			if(ts == p->first)
				value_resolve_conflict(p->second, ptr);
			delete ptr;
		}
	}

	virtual void free(void* item)
	{
		value_pair* old = static_cast<value_pair*>(item);
		if(old)
		{
			delete old->second;
			delete old;
		}
	}
};

class TSBoolExtItem : public TSGenericExtItem<bool>
{
	const bool conflict_value;
 protected:
	virtual std::string value_serialize(SerializeFormat format, const bool* value) const
	{
		if(format == FORMAT_USER)
			return *value ? "true" : "false";
		return *value ? "1" : "0";
	}

	virtual bool* value_unserialize(SerializeFormat format, const std::string& value)
	{
		if(value.empty())
			return new bool(default_value);
		if(value[0] == '0')
			return new bool(false);
		return new bool(true);
	}

	virtual void value_resolve_conflict(bool* value, const bool* newvalue)
	{
		if(*value != *newvalue)
			*value = conflict_value;
	}

 public:
	TSBoolExtItem(const std::string& Key, bool default_val, bool conflict_val, Module* parent) : TSGenericExtItem<bool>(Key, default_val, parent), conflict_value(conflict_val)
	{
	}
};

class TSStringExtItem : public TSGenericExtItem<std::string>
{
 protected:
	virtual std::string value_serialize(SerializeFormat format, const std::string* value) const
	{
		return *value;
	}

	virtual std::string* value_unserialize(SerializeFormat format, const std::string& value)
	{
		return new std::string(value);
	}

	virtual void value_resolve_conflict(std::string* value, const std::string* newvalue)
	{
		if(*value < *newvalue)
			*value = *newvalue;
	}

 public:
	TSStringExtItem(const std::string& Key, const std::string& default_val, Module* parent) : TSGenericExtItem<std::string>(Key, default_val, parent)
	{
	}
};

#endif
