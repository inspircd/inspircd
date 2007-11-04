/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __XLINE_H
#define __XLINE_H

#include <string>
#include <deque>
#include <vector>

/** XLine is the base class for ban lines such as G lines and K lines.
 * Modules may derive from this, and their xlines will automatically be
 * handled as expected by any protocol modules (e.g. m_spanningtree will
 * propogate them using AddLine). The process of translating a type+pattern
 * to a known line type is done by means of an XLineFactory object (see
 * below).
 */
class CoreExport XLine : public classbase
{
 protected:

	/** Creator */
	InspIRCd* ServerInstance;

	/** Default 'apply' action. Quits the user.
	 * @param u User to apply the line against
	 * @param line The line typed, used for display purposes in the quit message
	 * @param bancache If true, the user will be added to the bancache if they match. Else not.
	 */
	void DefaultApply(User* u, const std::string &line, bool bancache);

 public:

	/** Create an XLine.
	 * @param s_time The set time
	 * @param d The duration of the xline
	 * @param src The sender of the xline
	 * @param re The reason of the xline
	 * @param t The line type, should be set by the derived class constructor
	 */
	XLine(InspIRCd* Instance, time_t s_time, long d, const char* src, const char* re, const std::string &t)
		: ServerInstance(Instance), set_time(s_time), duration(d), type(t)
	{
		source = strdup(src);
		reason = strdup(re);
		expiry = set_time + duration;
	}

	/** Destructor
	 */
	virtual ~XLine()
	{
		free(reason);
		free(source);
	}

	/** Change creation time of an xline. Updates expiry
	 * to be after the creation time
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
	virtual bool Matches(User *u) = 0;

	/** Returns true whether or not the given string is covered by this line.
	 * @param str The string to match against. The details of what must be
	 * in this string and the mechanics of the match are defined by the
	 * derived class.
	 * @return True if there is a match
	 */
	virtual bool Matches(const std::string &str) = 0;

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
	 * line. Usually a line in the form 'expiring Xline blah, set by...'
	 * see the DisplayExpiry methods of GLine, ELine etc.
	 */
	virtual void DisplayExpiry() = 0;

	/** Returns the displayable form of the pattern for this xline,
	 * e.g. '*@foo' or '*baz*'. This must always return the full pattern
	 * in a form which can be used to construct an entire derived xline,
	 * even if it is stored differently internally (e.g. GLine stores the
	 * ident and host parts seperately but will still return ident@host
	 * for its Displayable() method)
	 */
	virtual const char* Displayable() = 0;

	/** Called when the xline has just been added.
	 */
	virtual void OnAdd() { }

	/** The time the line was added.
	 */
	time_t set_time;
	
	/** The duration of the ban, or 0 if permenant
	 */
	long duration;
	
	/** Source of the ban. This can be a servername or an oper nickname
	 */
	char* source;
	
	/** Reason for the ban
	 */
	char* reason;

	/** Expiry time. Does not contain useful data if the duration is 0.
	 */
	time_t expiry;

	/** "Q", "K", etc. Set only by derived classes constructor to the
	 * type of line this is.
	 */
	const std::string type;
};

/** KLine class
 */
class CoreExport KLine : public XLine
{
  public:

	/** Create a K-Line.
	 * @param s_time The set time
	 * @param d The duration of the xline
	 * @param src The sender of the xline
	 * @param re The reason of the xline
	 * @param ident Ident to match
	 * @param host Host to match
	 */
	KLine(InspIRCd* Instance, time_t s_time, long d, const char* src, const char* re, const char* ident, const char* host) : XLine(Instance, s_time, d, src, re, "K")
	{
		identmask = strdup(ident);
		hostmask = strdup(host);
		matchtext = this->identmask;
		matchtext.append("@").append(this->hostmask);
	}

	/** Destructor
	 */
	~KLine()
	{
		free(identmask);
		free(hostmask);
	}

	virtual bool Matches(User *u);

	virtual bool Matches(const std::string &str);

	virtual void Apply(User* u);

	virtual void DisplayExpiry();

	virtual const char* Displayable();

	/** Ident mask (ident part only)
	 */
	char* identmask;
	/** Host mask (host part only)
	 */
	char* hostmask;

	std::string matchtext;
};

/** GLine class
 */
class CoreExport GLine : public XLine
{
  public:
	/** Create a G-Line.
	 * @param s_time The set time
	 * @param d The duration of the xline
	 * @param src The sender of the xline
	 * @param re The reason of the xline
	 * @param ident Ident to match
	 * @param host Host to match
	 */
	GLine(InspIRCd* Instance, time_t s_time, long d, const char* src, const char* re, const char* ident, const char* host) : XLine(Instance, s_time, d, src, re, "G")
	{
		identmask = strdup(ident);
		hostmask = strdup(host);
		matchtext = this->identmask;
		matchtext.append("@").append(this->hostmask);
	}

	/** Destructor
	 */
	~GLine()
	{
		free(identmask);
		free(hostmask);
	}

	virtual bool Matches(User *u);

	virtual bool Matches(const std::string &str);

	virtual void Apply(User* u);

	virtual void DisplayExpiry();

	virtual const char* Displayable();

	/** Ident mask (ident part only)
	 */
	char* identmask;
	/** Host mask (host part only)
	 */
	char* hostmask;

	std::string matchtext;
};

/** ELine class
 */
class CoreExport ELine : public XLine
{
  public:
	/** Create an E-Line.
	 * @param s_time The set time
	 * @param d The duration of the xline
	 * @param src The sender of the xline
	 * @param re The reason of the xline
	 * @param ident Ident to match
	 * @param host Host to match
	 */
	ELine(InspIRCd* Instance, time_t s_time, long d, const char* src, const char* re, const char* ident, const char* host) : XLine(Instance, s_time, d, src, re, "E")
	{
		identmask = strdup(ident);
		hostmask = strdup(host);
		matchtext = this->identmask;
		matchtext.append("@").append(this->hostmask);
	}

	~ELine()
	{
		free(identmask);
		free(hostmask);
	}

	virtual bool Matches(User *u);

	virtual bool Matches(const std::string &str);

	virtual void Unset();

	virtual void DisplayExpiry();

	virtual void OnAdd();

	virtual const char* Displayable();

	/** Ident mask (ident part only)
	 */
	char* identmask;
	/** Host mask (host part only)
	 */
	char* hostmask;

	std::string matchtext;
};

/** ZLine class
 */
class CoreExport ZLine : public XLine
{
  public:
	/** Create a Z-Line.
	 * @param s_time The set time
	 * @param d The duration of the xline
	 * @param src The sender of the xline
	 * @param re The reason of the xline
	 * @param ip IP to match
	 */
	ZLine(InspIRCd* Instance, time_t s_time, long d, const char* src, const char* re, const char* ip) : XLine(Instance, s_time, d, src, re, "Z")
	{
		ipaddr = strdup(ip);
	}

	/** Destructor
	 */
	~ZLine()
	{
		free(ipaddr);
	}

	virtual bool Matches(User *u);

	virtual bool Matches(const std::string &str);

	virtual void Apply(User* u);

	virtual void DisplayExpiry();

	virtual const char* Displayable();

	/** IP mask (no ident part)
	 */
	char* ipaddr;
};

/** QLine class
 */
class CoreExport QLine : public XLine
{
  public:
	/** Create a G-Line.
	 * @param s_time The set time
	 * @param d The duration of the xline
	 * @param src The sender of the xline
	 * @param re The reason of the xline
	 * @param nickname Nickname to match
	 */
	QLine(InspIRCd* Instance, time_t s_time, long d, const char* src, const char* re, const char* nickname) : XLine(Instance, s_time, d, src, re, "Q")
	{
		nick = strdup(nickname);
	}

	/** Destructor
	 */
	~QLine()
	{
		free(nick);

	}
	virtual bool Matches(User *u);

	virtual bool Matches(const std::string &str);

	virtual void Apply(User* u);

	virtual void DisplayExpiry();

	virtual const char* Displayable();

	/** Nickname mask
	 */
	char* nick;
};

/** Contains an ident and host split into two strings
 */
typedef std::pair<std::string, std::string> IdentHostPair;

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

	InspIRCd* ServerInstance;
	std::string type;

 public:

	/** Create an XLine factory
	 * @param Instance creator
	 * @param t Type of XLine this factory generates
	 */
	XLineFactory(InspIRCd* Instance, const std::string &t) : ServerInstance(Instance), type(t) { }
	
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
	virtual XLine* Generate(time_t set_time, long duration, const char* source, const char* reason, const char* xline_specific_mask) = 0;

	/** Destructor
	 */
	virtual ~XLineFactory() { }
};

/* Required forward declarations
 */
class ServerConfig;
class InspIRCd;

class GLineFactory;
class ELineFactory;
class QLineFactory;
class ZLineFactory;
class KLineFactory;

/** A map of xline factories
 */
typedef std::map<std::string, XLineFactory*> XLineFactMap;

/** A map of XLines indexed by string
 */
typedef std::map<std::string, XLine *> XLineLookup;

/** A map of XLineLookup maps indexed by string
 */
typedef std::map<std::string, XLineLookup > XLineContainer;

/** An iterator in an XLineContainer
 */
typedef XLineContainer::iterator ContainerIter;

/** An interator in an XLineLookup
 */
typedef XLineLookup::iterator LookupIter;

/** XLineManager is a class used to manage glines, klines, elines, zlines and qlines,
 * or any other line created by a module. It also manages XLineFactory classes which
 * can generate a specialized XLine for use by another module.
 */
class CoreExport XLineManager
{
 protected:
	/** The owner/creator of this class
	 */
	InspIRCd* ServerInstance;

	/** Used to hold XLines which have not yet been applied.
	 */
	std::vector<XLine *> pending_lines;

	/** Current xline factories
	 */
	XLineFactMap line_factory;

	/** Core xline factories for G/E/K/Q/Z lines
	 * (These generate GLine, ELine, KLine, QLine and ZLine
	 * respectively)
	 */
	GLineFactory* GFact;
	ELineFactory* EFact;
	KLineFactory* KFact;
	QLineFactory* QFact;
	ZLineFactory* ZFact;

	/** Container of all lines, this is a map of maps which
	 * allows for fast lookup for add/remove of a line, and
	 * the shortest possible timed O(n) for checking a user
	 * against a line.
	 */
	XLineContainer lookup_lines;

 public:

	/** Constructor
	 * @param Instance A pointer to the creator object
	 */
	XLineManager(InspIRCd* Instance);

	/** Destructor
	 */
	~XLineManager();

	/** Split an ident and host into two seperate strings.
	 * This allows for faster matching.
	 */
	IdentHostPair IdentSplit(const std::string &ident_and_host);

	/** Checks what users match e:lines and sets their ban exempt flag accordingly.
	 */
	void CheckELines();

	/** Get all lines of a certain type to an XLineLookup (std::map<std::string, XLine*>).
	 * NOTE: When this function runs any expired items are removed from the list before it
	 * is returned to the caller.
	 * @param The type to look up
	 * @return A list of all XLines of the given type.
	 */
	XLineLookup* GetAll(const std::string &type);

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
	 * @param user The user removing the line or NULL if its the local server
	 * @param simulate If this is true, don't actually remove the line, just return
	 * @return True if the line was deleted successfully
	 */
	bool DelLine(const char* hostmask, const std::string &type, User* user, bool simulate = false);

	/** Registers an xline factory.
	 * An xline factory is a class which when given a particular xline type,
	 * will generate a new XLine specialized to that type. For example if you
	 * pass the XLineFactory that handles glines some data it will return a
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
	XLineFactory* GetFactory(const std::string &type);

	/** Check if a user matches an XLine
	 * @param type The type of line to look up
	 * @param user The user to match against (what is checked is specific to the xline type)
	 * @return The reason for the line if there is a match, or NULL if there is no match
	 */
	XLine* MatchesLine(const std::string &type, User* user);

	/** Check if a pattern matches an XLine
	 * @param type The type of line to look up
	 * @param pattern A pattern string specific to the xline type
	 * @return The matching XLine if there is a match, or NULL if there is no match
	 */
	XLine* MatchesLine(const std::string &type, const std::string &pattern);

	/** Expire a line given two iterators which identify it in the main map.
	 * @param container Iterator to the first level of entries the map
	 * @param item Iterator to the second level of entries in the map
	 */
	void ExpireLine(ContainerIter container, LookupIter item);

	/** Apply any new lines that are pending to be applied.
	 * This will only apply lines in the pending_lines list, to save on
	 * CPU time.
	 */
	void ApplyLines();

	/** Handle /STATS for a given type.
	 * NOTE: Any items in the list for this particular line type which have expired
	 * will be expired and removed before the list is displayed.
	 * @param numeric The numeric to give to each result line
	 * @param user The username making the query
	 * @param results The string_list to receive the results
	 */
	void InvokeStats(const std::string &type, int numeric, User* user, string_list &results);
};

/** An XLineFactory specialized to generate GLine* pointers
 */
class CoreExport GLineFactory : public XLineFactory
{
 public:
	GLineFactory(InspIRCd* Instance) : XLineFactory(Instance, "G") { }

	/** Generate a GLine
	 */
	XLine* Generate(time_t set_time, long duration, const char* source, const char* reason, const char* xline_specific_mask)
	{
		IdentHostPair ih = ServerInstance->XLines->IdentSplit(xline_specific_mask);
		return new GLine(ServerInstance, set_time, duration, source, reason, ih.first.c_str(), ih.second.c_str());
	}
};

/** An XLineFactory specialized to generate ELine* pointers
 */
class CoreExport ELineFactory : public XLineFactory
{
 public:
	ELineFactory(InspIRCd* Instance) : XLineFactory(Instance, "E") { }

	/** Generate an ELine
	 */
	XLine* Generate(time_t set_time, long duration, const char* source, const char* reason, const char* xline_specific_mask)
	{
		IdentHostPair ih = ServerInstance->XLines->IdentSplit(xline_specific_mask);
		return new ELine(ServerInstance, set_time, duration, source, reason, ih.first.c_str(), ih.second.c_str());
	}
};

/** An XLineFactory specialized to generate KLine* pointers
 */
class CoreExport KLineFactory : public XLineFactory
{
 public:
        KLineFactory(InspIRCd* Instance) : XLineFactory(Instance, "K") { }

	/** Generate a KLine
	 */
        XLine* Generate(time_t set_time, long duration, const char* source, const char* reason, const char* xline_specific_mask)
        {
                IdentHostPair ih = ServerInstance->XLines->IdentSplit(xline_specific_mask);
                return new KLine(ServerInstance, set_time, duration, source, reason, ih.first.c_str(), ih.second.c_str());
        }
};

/** An XLineFactory specialized to generate QLine* pointers
 */
class CoreExport QLineFactory : public XLineFactory
{
 public:
        QLineFactory(InspIRCd* Instance) : XLineFactory(Instance, "Q") { }

	/** Generate a QLine
	 */
        XLine* Generate(time_t set_time, long duration, const char* source, const char* reason, const char* xline_specific_mask)
        {
                return new QLine(ServerInstance, set_time, duration, source, reason, xline_specific_mask);
        }
};

/** An XLineFactory specialized to generate ZLine* pointers
 */
class CoreExport ZLineFactory : public XLineFactory
{
 public:
        ZLineFactory(InspIRCd* Instance) : XLineFactory(Instance, "Z") { }

	/** Generate a ZLine
	 */
        XLine* Generate(time_t set_time, long duration, const char* source, const char* reason, const char* xline_specific_mask)
        {
                return new ZLine(ServerInstance, set_time, duration, source, reason, xline_specific_mask);
        }
};

#endif

