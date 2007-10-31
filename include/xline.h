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

// include the common header files

#include <string>
#include <deque>
#include <vector>
#include "users.h"
#include "channels.h"

const int APPLY_GLINES		= 1;
const int APPLY_KLINES		= 2;
const int APPLY_QLINES		= 4;
const int APPLY_ZLINES		= 8;
const int APPLY_PERM_ONLY	= 16;
const int APPLY_ALL		= APPLY_GLINES | APPLY_KLINES | APPLY_QLINES | APPLY_ZLINES;

/** XLine is the base class for ban lines such as G lines and K lines.
 */
class CoreExport XLine : public classbase
{
 protected:

	InspIRCd* ServerInstance;
	void DefaultApply(User* u, char line);

 public:

	/** Create an XLine.
	 * @param s_time The set time
	 * @param d The duration of the xline
	 * @param src The sender of the xline
	 * @param re The reason of the xline
	 */
	XLine(InspIRCd* Instance, time_t s_time, long d, const char* src, const char* re, const char t)
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

	/** Returns true whether or not the given user is covered by this line.
	 */
	virtual bool Matches(User *u) = 0;

	/** Returns true wether or not the given string exactly matches the gline
	 * (no wildcard use in this method) -- used for removal of a line
	 */
	virtual bool MatchesLiteral(const std::string &str) = 0;

	virtual bool Matches(const std::string &str) = 0;

	virtual void Apply(User* u);

	virtual void Unset() { }

	virtual void DisplayExpiry() = 0;

	virtual const char* Displayable() = 0;

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

	/** Expiry time
	 */
	time_t expiry;

	/** Q, K, etc. Don't change this. Constructors set it.
	 */
	const char type;
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
	KLine(InspIRCd* Instance, time_t s_time, long d, const char* src, const char* re, const char* ident, const char* host) : XLine(Instance, s_time, d, src, re, 'K')
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

	virtual bool MatchesLiteral(const std::string &str);

	virtual void Apply(User* u);

	virtual void DisplayExpiry();

	virtual const char* Displayable();

	/** Ident mask
	 */
	char* identmask;
	/** Host mask
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
	GLine(InspIRCd* Instance, time_t s_time, long d, const char* src, const char* re, const char* ident, const char* host) : XLine(Instance, s_time, d, src, re, 'G')
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

	virtual bool MatchesLiteral(const std::string &str);

	virtual void Apply(User* u);

	virtual void DisplayExpiry();

	virtual const char* Displayable();

	/** Ident mask
	 */
	char* identmask;
	/** Host mask
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
	ELine(InspIRCd* Instance, time_t s_time, long d, const char* src, const char* re, const char* ident, const char* host) : XLine(Instance, s_time, d, src, re, 'E')
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

	virtual bool MatchesLiteral(const std::string &str);

	virtual void Unset();

	virtual void DisplayExpiry();

	virtual void OnAdd();

	virtual const char* Displayable();

	/** Ident mask
	 */
	char* identmask;
	/** Host mask
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
	ZLine(InspIRCd* Instance, time_t s_time, long d, const char* src, const char* re, const char* ip) : XLine(Instance, s_time, d, src, re, 'Z')
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

	virtual bool MatchesLiteral(const std::string &str);

	virtual void Apply(User* u);

	virtual void DisplayExpiry();

	virtual const char* Displayable();

	/** IP mask
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
	QLine(InspIRCd* Instance, time_t s_time, long d, const char* src, const char* re, const char* nickname) : XLine(Instance, s_time, d, src, re, 'Q')
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

	virtual bool MatchesLiteral(const std::string &str);

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


class CoreExport XLineFactory
{
 protected:

	InspIRCd* ServerInstance;
	const char type;

 public:

	XLineFactory(InspIRCd* Instance, const char t) : ServerInstance(Instance), type(t) { }
	
	virtual const char GetType() { return type; }

	virtual XLine* Generate(time_t set_time, long duration, const char* source, const char* reason, const char* xline_specific_mask) = 0;

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

/** XLineManager is a class used to manage glines, klines, elines, zlines and qlines.
 */
class CoreExport XLineManager
{
 protected:
	/** The owner/creator of this class
	 */
	InspIRCd* ServerInstance;

	/** This functor is used by the std::sort() function to keep all lines in order
	 */
	static bool XSortComparison (const XLine *one, const XLine *two);

	/** Used to hold XLines which have not yet been applied.
	 */
	std::vector<XLine *> pending_lines;

	std::vector<XLine *> active_lines;

	std::map<char, XLineFactory*> line_factory;

	GLineFactory* GFact;
	ELineFactory* EFact;
	KLineFactory* KFact;
	QLineFactory* QFact;
	ZLineFactory* ZFact;

 public:

	std::map<char, std::map<std::string, XLine *> > lookup_lines;

	/** Constructor
	 * @param Instance A pointer to the creator object
	 */
	XLineManager(InspIRCd* Instance);

	~XLineManager();

	/** Split an ident and host into two seperate strings.
	 * This allows for faster matching.
	 */
	IdentHostPair IdentSplit(const std::string &ident_and_host);

	/** Checks what users match a given list of ELines and sets their ban exempt flag accordingly.
	 * @param ELines List of E:Lines to check.
	 */
	void CheckELines(std::map<std::string, XLine *> &ELines);

	/** Add a new GLine
	 * @param duration The duration of the line
	 * @param source The source of the line
	 * @param reason The reason for the line
	 * @param hostmask The hostmask
	 * @return True if the line was added successfully
	 */
	bool AddLine(XLine* line);

	/** Delete a GLine
	 * @param hostmask The host to remove
	 * @param type Type of line to remove
	 * @param simulate If this is true, don't actually remove the line, just return
	 * @return True if the line was deleted successfully
	 */
	bool DelLine(const char* hostmask, char type, bool simulate = false);

	/** Registers an xline factory.
	 * An xline factory is a class which when given a particular xline type,
	 * will generate a new XLine specialized to that type. For example if you
	 * pass the XLineFactory that handles glines some data it will return a
	 * pointer to a GLine, polymorphically represented as XLine. This is used where
	 * you do not know the full details of the item you wish to create, e.g. in a 
	 * server protocol module like m_spanningtree, when you receive xlines from other
	 * servers.
	 */
	bool RegisterFactory(XLineFactory* xlf);

	/** Unregisters an xline factory
	 */
	bool UnregisterFactory(XLineFactory* xlf);

	/** Get the XLineFactory for a specific type.
	 * Returns NULL if there is no known handler for this xline type
	 */
	XLineFactory* GetFactory(const char type);

	/** Check if a nickname matches a QLine
	 * @return nick The nick to check against
	 * @return The reason for the line if there is a match, or NULL if there is no match
	 */
	QLine* matches_qline(const char* nick);

	/** Check if a hostname matches a GLine
	 * @param user The user to check against
	 * @return The reason for the line if there is a match, or NULL if there is no match
	 */
	GLine* matches_gline(User* user);

	/** Check if a user's IP matches a ZLine
	 * @param user The user to check against
	 * @return The reason for the line if there is a match, or NULL if there is no match
	 */
	ZLine* matches_zline(User *user);

	/** Check if a hostname matches a KLine
	 * @param user The user to check against
	 * @return The reason for the line if there is a match, or NULL if there is no match
	 */
	KLine* matches_kline(User* user);

	/** Check if a hostname matches a ELine
	 * @param user The user to check against
	 * @return The reason for the line if there is a match, or NULL if there is no match
	 */
	ELine* matches_exception(User* user);

	/** Expire any lines that should be expired.
	 */
	void expire_lines();

	/** Apply any new lines that are pending to be applied
	 */
	void ApplyLines();

	/** Handle /STATS K
	 * @param user The username making the query
	 * @param results The string_list to receive the results
	 */
	void stats_k(User* user, string_list &results);

	/** Handle /STATS G
	 * @param user The username making the query
	 * @param results The string_list to receive the results
	 */
	void stats_g(User* user, string_list &results);

	/** Handle /STATS Q
	 * @param user The username making the query
	 * @param results The string_list to receive the results
	 */
	void stats_q(User* user, string_list &results);

	/** Handle /STATS Z
	 * @param user The username making the query
	 * @param results The string_list to receive the results
	 */
	void stats_z(User* user, string_list &results);

	/** Handle /STATS E
	 * @param user The username making the query
	 * @param results The string_list to receive the results
	 */
	void stats_e(User* user, string_list &results);

	/** Change creation time of a GLine
	 * @param host The hostname to change
	 * @param create_Time The new creation time
	 */
	void gline_set_creation_time(const char* host, time_t create_time);

	/** Change creation time of a QLine
	 * @param nick The nickmask to change
	 * @param create_Time The new creation time
	 */
	void qline_set_creation_time(const char* nick, time_t create_time);

	/** Change creation time of a ZLine
	 * @param ip The ipmask to change
	 * @param create_Time The new creation time
	 */
	void zline_set_creation_time(const char* ip, time_t create_time);

	/** Change creation time of a ELine
	 * @param host The hostname to change
	 * @param create_Time The new creation time
	 */
	void eline_set_creation_time(const char* host, time_t create_time);
};

class CoreExport GLineFactory : public XLineFactory
{
 public:
	GLineFactory(InspIRCd* Instance) : XLineFactory(Instance, 'G') { }

	XLine* Generate(time_t set_time, long duration, const char* source, const char* reason, const char* xline_specific_mask)
	{
		IdentHostPair ih = ServerInstance->XLines->IdentSplit(xline_specific_mask);
		return new GLine(ServerInstance, set_time, duration, source, reason, ih.first.c_str(), ih.second.c_str());
	}
};

class CoreExport ELineFactory : public XLineFactory
{
 public:
	ELineFactory(InspIRCd* Instance) : XLineFactory(Instance, 'E') { }

	XLine* Generate(time_t set_time, long duration, const char* source, const char* reason, const char* xline_specific_mask)
	{
		IdentHostPair ih = ServerInstance->XLines->IdentSplit(xline_specific_mask);
		return new ELine(ServerInstance, set_time, duration, source, reason, ih.first.c_str(), ih.second.c_str());
	}
};

class CoreExport KLineFactory : public XLineFactory
{
 public:
        KLineFactory(InspIRCd* Instance) : XLineFactory(Instance, 'K') { }

        XLine* Generate(time_t set_time, long duration, const char* source, const char* reason, const char* xline_specific_mask)
        {
                IdentHostPair ih = ServerInstance->XLines->IdentSplit(xline_specific_mask);
                return new KLine(ServerInstance, set_time, duration, source, reason, ih.first.c_str(), ih.second.c_str());
        }
};

class CoreExport QLineFactory : public XLineFactory
{
 public:
        QLineFactory(InspIRCd* Instance) : XLineFactory(Instance, 'Q') { }

        XLine* Generate(time_t set_time, long duration, const char* source, const char* reason, const char* xline_specific_mask)
        {
                return new QLine(ServerInstance, set_time, duration, source, reason, xline_specific_mask);
        }
};

class CoreExport ZLineFactory : public XLineFactory
{
 public:
        ZLineFactory(InspIRCd* Instance) : XLineFactory(Instance, 'Z') { }

        XLine* Generate(time_t set_time, long duration, const char* source, const char* reason, const char* xline_specific_mask)
        {
                return new ZLine(ServerInstance, set_time, duration, source, reason, xline_specific_mask);
        }
};

#endif
