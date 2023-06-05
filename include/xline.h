/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2012-2013, 2017-2018, 2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2004, 2006-2008 Craig Edwards <brain@inspircd.org>
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


#pragma once

namespace Stats
{
	class Context;
}

/** Contains an ident and host split into two strings
 */
typedef std::pair<std::string, std::string> IdentHostPair;

/** A map of xline factories
 */
typedef std::map<std::string, XLineFactory*> XLineFactMap;

/** A map of XLines indexed by string
 */
typedef std::map<std::string, XLine*, irc::insensitive_swo> XLineLookup;

/** A map of XLineLookup maps indexed by string
 */
typedef std::map<std::string, XLineLookup > XLineContainer;

/** An iterator in an XLineContainer
 */
typedef XLineContainer::iterator ContainerIter;

/** An iterator in an XLineLookup
 */
typedef XLineLookup::iterator LookupIter;

/** XLine is the base class for ban lines such as G-lines and K-lines.
 * Modules may derive from this, and their xlines will automatically be
 * handled as expected by any protocol modules (e.g. m_spanningtree will
 * propagate them using AddLine). The process of translating a type+pattern
 * to a known line type is done by means of an XLineFactory object (see
 * below).
 */
class CoreExport XLine
	: public Cullable
{
protected:

	/** Default 'apply' action. Quits the user.
	 * @param u User to apply the line against
	 * @param line The line typed, used for display purposes in the quit message
	 * @param bancache If true, the user will be added to the bancache if they match. Else not.
	 */
	void DefaultApply(User* u, const std::string& line, bool bancache);

public:

	/** Create an XLine.
	 * @param s_time The set time
	 * @param d The duration of the xline
	 * @param src The sender of the xline
	 * @param re The reason of the xline
	 * @param t The line type, should be set by the derived class constructor
	 */
	XLine(time_t s_time, unsigned long d, const std::string& src, const std::string& re, const std::string& t)
		: set_time(s_time)
		, duration(d)
		, source(src)
		, reason(re)
		, type(t)
	{
		expiry = set_time + duration;
	}

	/** Change creation time of an xline. Updates expiry
	 * to be after the creation time.
	 */
	virtual void SetCreateTime(time_t created)
	{
		set_time = created;
		expiry = created + duration;
	}

	/** Returns true whether or not the given user is covered by this line.
	 * @param u The user to match against. The mechanics of the match
	 * are defined by the derived class.
	 * @return True if there is a match.
	 */
	virtual bool Matches(User* u) const = 0;

	/** Returns true whether or not the given string is covered by this line.
	 * @param str The string to match against. The details of what must be
	 * in this string and the mechanics of the match are defined by the
	 * derived class.
	 * @return True if there is a match
	 */
	virtual bool Matches(const std::string& str) const = 0;

	/** Apply a line against a user. The mechanics of what occurs when
	 * the line is applied are specific to the derived class.
	 * @param u The user to apply against
	 */
	virtual void Apply(User* u);

	/** Called when the line is unset either via expiry or
	 * via explicit removal.
	 */
	virtual void Unset() { }

	/** Called when the expiry message is to be displayed for the
	 * line. Usually a line in the form 'expiring X-line blah, set by...'
	 * see the DisplayExpiry methods of GLine, ELine etc.
	 */
	virtual void DisplayExpiry();

	/** Returns the displayable form of the pattern for this xline,
	 * e.g. '*\@foo' or '*baz*'. This must always return the full pattern
	 * in a form which can be used to construct an entire derived xline,
	 * even if it is stored differently internally (e.g. GLine stores the
	 * ident and host parts separately but will still return ident\@host
	 * for its Displayable() method).
	 */
	virtual const std::string& Displayable() const = 0;

	/** Called when the xline has just been added.
	 */
	virtual void OnAdd() { }

	/** The time the line was added.
	 */
	time_t set_time;

	/** The duration of the ban, or 0 if permanent
	 */
	unsigned long duration;

	/** Source of the ban. This can be a servername or an oper nickname
	 */
	std::string source;

	/** Reason for the ban
	 */
	std::string reason;

	/** Expiry time. Does not contain useful data if the duration is 0.
	 */
	time_t expiry;

	/** "Q", "K", etc. Set only by derived classes constructor to the
	 * type of line this is.
	 */
	const std::string type;

	// Whether this XLine was loaded from the server config.
	bool from_config = false;

	virtual bool IsBurstable();
};

/** KLine class
 */
class CoreExport KLine final
	: public XLine
{
public:

	/** Create a K-line.
	 * @param s_time The set time
	 * @param d The duration of the xline
	 * @param src The sender of the xline
	 * @param re The reason of the xline
	 * @param ident Ident to match
	 * @param host Host to match
	 */
	KLine(time_t s_time, unsigned long d, const std::string& src, const std::string& re, const std::string& ident, const std::string& host)
		: XLine(s_time, d, src, re, "K")
		, identmask(ident)
		, hostmask(host)
	{
		matchtext = this->identmask;
		matchtext.append("@").append(this->hostmask);
	}

	bool Matches(User* u) const override;

	bool Matches(const std::string& str) const override;

	void Apply(User* u) override;

	const std::string& Displayable() const override;

	bool IsBurstable() override;

	/** Ident mask (ident part only)
	 */
	std::string identmask;
	/** Host mask (host part only)
	 */
	std::string hostmask;

	std::string matchtext;
};

/** GLine class
 */
class CoreExport GLine final
	: public XLine
{
public:
	/** Create a G-line.
	 * @param s_time The set time
	 * @param d The duration of the xline
	 * @param src The sender of the xline
	 * @param re The reason of the xline
	 * @param ident Ident to match
	 * @param host Host to match
	 */
	GLine(time_t s_time, unsigned long d, const std::string& src, const std::string& re, const std::string& ident, const std::string& host)
		: XLine(s_time, d, src, re, "G")
		, identmask(ident)
		, hostmask(host)
	{
		matchtext = this->identmask;
		matchtext.append("@").append(this->hostmask);
	}

	bool Matches(User* u) const override;

	bool Matches(const std::string& str) const override;

	void Apply(User* u)  override;

	const std::string& Displayable() const override;

	/** Ident mask (ident part only)
	 */
	std::string identmask;
	/** Host mask (host part only)
	 */
	std::string hostmask;

	std::string matchtext;
};

/** ELine class
 */
class CoreExport ELine final
	: public XLine
{
public:
	/** Create an E-line.
	 * @param s_time The set time
	 * @param d The duration of the xline
	 * @param src The sender of the xline
	 * @param re The reason of the xline
	 * @param ident Ident to match
	 * @param host Host to match
	 */
	ELine(time_t s_time, unsigned long d, const std::string& src, const std::string& re, const std::string& ident, const std::string& host)
		: XLine(s_time, d, src, re, "E")
		, identmask(ident)
		, hostmask(host)
	{
		matchtext = this->identmask;
		matchtext.append("@").append(this->hostmask);
	}

	bool Matches(User* u) const override;

	bool Matches(const std::string& str) const override;

	void Unset() override;

	void OnAdd() override;

	const std::string& Displayable() const override;

	/** Ident mask (ident part only)
	 */
	std::string identmask;
	/** Host mask (host part only)
	 */
	std::string hostmask;

	std::string matchtext;
};

/** ZLine class
 */
class CoreExport ZLine final
	: public XLine
{
public:
	/** Create a Z-line.
	 * @param s_time The set time
	 * @param d The duration of the xline
	 * @param src The sender of the xline
	 * @param re The reason of the xline
	 * @param ip IP to match
	 */
	ZLine(time_t s_time, unsigned long d, const std::string& src, const std::string& re, const std::string& ip)
		: XLine(s_time, d, src, re, "Z")
		, ipaddr(ip)
	{
	}

	bool Matches(User* u) const override;

	bool Matches(const std::string& str) const override;

	void Apply(User* u) override;

	const std::string& Displayable() const override;

	/** IP mask (no ident part)
	 */
	std::string ipaddr;
};

/** QLine class
 */
class CoreExport QLine final
	: public XLine
{
public:
	/** Create a Q-line.
	 * @param s_time The set time
	 * @param d The duration of the xline
	 * @param src The sender of the xline
	 * @param re The reason of the xline
	 * @param nickname Nickname to match
	 */
	QLine(time_t s_time, unsigned long d, const std::string& src, const std::string& re, const std::string& nickname)
		: XLine(s_time, d, src, re, "Q")
		, nick(nickname)
	{
	}

	bool Matches(User* u) const override;

	bool Matches(const std::string& str) const override;

	void Apply(User* u) override;

	const std::string& Displayable() const override;

	/** Nickname mask
	 */
	std::string nick;
};

/** XLineFactory is used to generate an XLine pointer, given just the
 * pattern, timing information and type of line to create. This is used
 * for example in the spanningtree module which will call an XLineFactory
 * to create a new XLine when it is inbound on a server link, so that it
 * does not have to know the specifics of the internals of an XLine class
 * and/or how to call its constructor.
 */
class CoreExport XLineFactory
{
protected:

	std::string type;

public:

	/** Create an XLine factory
	 * @param t Type of XLine this factory generates
	 */
	XLineFactory(const std::string& t)
		: type(t)
	{
	}

	/** Return the type of XLine this factory generates
	 * @return The type of XLine this factory generates
	 */
	virtual const std::string& GetType() { return type; }

	/** Generate a specialized XLine*.
	 * @param set_time Time this line was created
	 * @param duration Duration of the line
	 * @param source The sender of the line, nickname or server
	 * @param reason The reason for the line
	 * @param xline_specific_mask The mask string for the line, specific to the XLine type being created.
	 * @return A specialized XLine class of the given type for this factory.
	 */
	virtual XLine* Generate(time_t set_time, unsigned long duration, const std::string& source, const std::string& reason, const std::string& xline_specific_mask) = 0;

	virtual bool AutoApplyToUserList(XLine* x) { return true; }

	/** Destructor
	 */
	virtual ~XLineFactory() = default;
};

/** XLineManager is a class used to manage G-lines, K-lines, E-lines, Z-lines and Q-lines,
 * or any other line created by a module. It also manages XLineFactory classes which
 * can generate a specialized XLine for use by another module.
 */
class CoreExport XLineManager final
{
protected:
	/** Used to hold XLines which have not yet been applied.
	 */
	std::vector<XLine *> pending_lines;

	/** Current xline factories
	 */
	XLineFactMap line_factory;

	/** Container of all lines, this is a map of maps which
	 * allows for fast lookup for add/remove of a line, and
	 * the shortest possible timed O(n) for checking a user
	 * against a line.
	 */
	XLineContainer lookup_lines;

public:

	/** Constructor
	 */
	XLineManager();

	/** Destructor
	 */
	~XLineManager();

	/** Split an ident and host into two separate strings.
	 * This allows for faster matching.
	 */
	static IdentHostPair IdentSplit(const std::string& ident_and_host);

	/** Checks what users match E-lines and sets their ban exempt flag accordingly.
	 */
	void CheckELines();

	/** Get all lines of a certain type to an XLineLookup (std::map<std::string, XLine*>).
	 * NOTE: When this function runs any expired items are removed from the list before it
	 * is returned to the caller.
	 * @param type The type to look up
	 * @return A list of all XLines of the given type.
	 */
	XLineLookup* GetAll(const std::string& type);

	/** Remove all lines of a certain type.
	 */
	void DelAll(const std::string& type);

	/** Return all known types of line currently stored by the XLineManager.
	 * @return A vector containing all known line types currently stored in the main list.
	 */
	std::vector<std::string> GetAllTypes();

	/** Add a new XLine
	 * @param line The line to be added
	 * @param user The user adding the line or NULL for the local server
	 * @return True if the line was added successfully
	 */
	bool AddLine(XLine* line, User* user);

	/** Delete an XLine
	 * @param hostmask The xline-specific string identifying the line, e.g. "*@foo"
	 * @param type The type of xline
	 * @param reason The xline reason, if it is being removed successfully
	 * @param user The user removing the line or NULL if its the local server
	 * @param simulate If this is true, don't actually remove the line, just return
	 * @return True if the line was deleted successfully
	 */
	bool DelLine(const std::string& hostmask, const std::string& type, std::string& reason, User* user, bool simulate = false);

	/** Delete an XLine
	 * @param xline The xline to delete.
	 * @param reason The xline reason, if it is being removed successfully
	 * @param user The user removing the line or NULL if its the local server
	 * @param simulate If this is true, don't actually remove the line, just return
	 * @return True if the line was deleted successfully
	 */
	bool DelLine(const XLine* xline, std::string& reason, User* user, bool simulate = false)
	{
		return DelLine(xline->Displayable(), xline->type, reason, user, simulate);
	}

	/** Registers an xline factory.
	 * An xline factory is a class which when given a particular xline type,
	 * will generate a new XLine specialized to that type. For example if you
	 * pass the XLineFactory that handles G-lines some data it will return a
	 * pointer to a GLine, polymorphically represented as XLine. This is used where
	 * you do not know the full details of the item you wish to create, e.g. in a
	 * server protocol module like m_spanningtree, when you receive xlines from other
	 * servers.
	 * @param xlf XLineFactory pointer to register
	 */
	bool RegisterFactory(XLineFactory* xlf);

	/** Unregisters an xline factory.
	 * You must do this when your module unloads.
	 * @param xlf XLineFactory pointer to unregister
	 */
	bool UnregisterFactory(XLineFactory* xlf);

	/** Get the XLineFactory for a specific type.
	 * Returns NULL if there is no known handler for this xline type.
	 * @param type The type of XLine you require the XLineFactory for
	 */
	XLineFactory* GetFactory(const std::string& type);

	/** Check if a user matches an XLine
	 * @param type The type of line to look up
	 * @param user The user to match against (what is checked is specific to the xline type)
	 * @return The reason for the line if there is a match, or NULL if there is no match
	 */
	XLine* MatchesLine(const std::string& type, User* user);

	/** Check if a pattern matches an XLine
	 * @param type The type of line to look up
	 * @param pattern A pattern string specific to the xline type
	 * @return The matching XLine if there is a match, or NULL if there is no match
	 */
	XLine* MatchesLine(const std::string& type, const std::string& pattern);

	/** Expire a line given two iterators which identify it in the main map.
	 * @param container Iterator to the first level of entries the map
	 * @param item Iterator to the second level of entries in the map
	 * @param silent If true, doesn't send an expiry SNOTICE.
	 */
	void ExpireLine(ContainerIter container, LookupIter item, bool silent = false);

	/** Apply any new lines that are pending to be applied.
	 * This will only apply lines in the pending_lines list, to save on
	 * CPU time.
	 */
	void ApplyLines();

	/** Generates a /STATS response for the given X-line type.
	 * @param type The type of X-line to look up.
	 * @param context The stats context to respond with.
	 * @return True if a response was sent; otherwise, false.
	 */
	bool InvokeStats(const std::string& type, Stats::Context& context);

	/** Expire X-lines which were added by the server configuration and have been removed. */
	void ExpireRemovedConfigLines(const std::string& type, const insp::flat_set<std::string>& configlines);
};
