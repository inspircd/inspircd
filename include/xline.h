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
  public:

	/** Create an XLine.
	 * @param s_time The set time
	 * @param d The duration of the xline
	 * @param src The sender of the xline
	 * @param re The reason of the xline
	 */
	XLine(time_t s_time, long d, const char* src, const char* re)
		: set_time(s_time), duration(d)
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
	KLine(time_t s_time, long d, const char* src, const char* re, const char* ident, const char* host) : XLine(s_time, d, src, re)
	{
		identmask = strdup(ident);
		hostmask = strdup(host);
	}

	/** Destructor
	 */
	~KLine()
	{
		free(identmask);
		free(hostmask);
	}

	/** Ident mask
	 */
	char* identmask;
	/** Host mask
	 */
	char* hostmask;
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
	GLine(time_t s_time, long d, const char* src, const char* re, const char* ident, const char* host) : XLine(s_time, d, src, re)
	{
		identmask = strdup(ident);
		hostmask = strdup(host);
	}

	/** Destructor
	 */
	~GLine()
	{
		free(identmask);
		free(hostmask);
	}

	/** Ident mask
	 */
	char* identmask;
	/** Host mask
	 */
	char* hostmask;
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
	ELine(time_t s_time, long d, const char* src, const char* re, const char* ident, const char* host) : XLine(s_time, d, src, re)
	{
		identmask = strdup(ident);
		hostmask = strdup(host);
	}

	~ELine()
	{
		free(identmask);
		free(hostmask);
	}

	/** Ident mask
	 */
	char* identmask;
	/** Host mask
	 */
	char* hostmask;
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
	ZLine(time_t s_time, long d, const char* src, const char* re, const char* ip) : XLine(s_time, d, src, re)
	{
		ipaddr = strdup(ip);
	}

	/** Destructor
	 */
	~ZLine()
	{
		free(ipaddr);
	}

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
	QLine(time_t s_time, long d, const char* src, const char* re, const char* nickname) : XLine(s_time, d, src, re)
	{
		nick = strdup(nickname);
	}

	/** Destructor
	 */
	~QLine()
	{
		free(nick);
	}

	/** Nickname mask
	 */
	char* nick;
};

/* Required forward declarations
 */
class ServerConfig;
class InspIRCd;

/** Initialize x line
 */
bool InitXLine(ServerConfig* conf, const char* tag);

/** Done adding zlines from the config
 */
bool DoneZLine(ServerConfig* conf, const char* tag);
/** Done adding qlines from the config
 */
bool DoneQLine(ServerConfig* conf, const char* tag);
/** Done adding klines from the config
 */
bool DoneKLine(ServerConfig* conf, const char* tag);
/** Done adding elines from the config
 */
bool DoneELine(ServerConfig* conf, const char* tag);

/** Add a config-defined zline
 */
bool DoZLine(ServerConfig* conf, const char* tag, char** entries, ValueList &values, int* types);
/** Add a config-defined qline
 */
bool DoQLine(ServerConfig* conf, const char* tag, char** entries, ValueList &values, int* types);
/** Add a config-defined kline
 */
bool DoKLine(ServerConfig* conf, const char* tag, char** entries, ValueList &values, int* types);
/** Add a config-defined eline
 */
bool DoELine(ServerConfig* conf, const char* tag, char** entries, ValueList &values, int* types);

/** Contains an ident and host split into two strings
 */
typedef std::pair<std::string, std::string> IdentHostPair;

/** XLineManager is a class used to manage glines, klines, elines, zlines and qlines.
 */
class CoreExport XLineManager
{
 protected:
	/** The owner/creator of this class
	 */
	InspIRCd* ServerInstance;

	/** This functor is used by the std::sort() function to keep glines in order
	 */
	static bool GSortComparison ( const GLine* one, const GLine* two );

	/** This functor is used by the std::sort() function to keep elines in order
	 */
	static bool ESortComparison ( const ELine* one, const ELine* two );

	/** This functor is used by the std::sort() function to keep zlines in order
	 */
	static bool ZSortComparison ( const ZLine* one, const ZLine* two );

	/** This functor is used by the std::sort() function to keep klines in order
	 */
	static bool KSortComparison ( const KLine* one, const KLine* two );

	/** This functor is used by the std::sort() function to keep qlines in order
	 */
	static bool QSortComparison ( const QLine* one, const QLine* two );
 public:
	/* Lists for temporary lines with an expiry time */

	/** Temporary KLines */
	std::vector<KLine*> klines;

	/** Temporary Glines */
	std::vector<GLine*> glines;

	/** Temporary Zlines */
	std::vector<ZLine*> zlines;

	/** Temporary QLines */
	std::vector<QLine*> qlines;

	/** Temporary ELines */
	std::vector<ELine*> elines;

	/* Seperate lists for perm XLines that isnt checked by expiry functions */

	/** Permenant KLines */
	std::vector<KLine*> pklines;

	/** Permenant GLines */
	std::vector<GLine*> pglines;

	/** Permenant ZLines */
	std::vector<ZLine*> pzlines;

	/** Permenant QLines */
	std::vector<QLine*> pqlines;

	/** Permenant ELines */
	std::vector<ELine*> pelines;
	
	/** Constructor
	 * @param Instance A pointer to the creator object
	 */
	XLineManager(InspIRCd* Instance);

	/** Split an ident and host into two seperate strings.
	 * This allows for faster matching.
	 */
	IdentHostPair IdentSplit(const std::string &ident_and_host);

	/** Add a new GLine
	 * @param duration The duration of the line
	 * @param source The source of the line
	 * @param reason The reason for the line
	 * @param hostmask The hostmask
	 * @return True if the line was added successfully
	 */
	bool add_gline(long duration, const char* source, const char* reason, const char* hostmask);

	/** Add a new QLine
	 * @param duration The duration of the line
	 * @param source The source of the line
	 * @param reason The reason for the line
	 * @param nickname The nickmask
	 * @return True if the line was added successfully
	 */
	bool add_qline(long duration, const char* source, const char* reason, const char* nickname);

	/** Add a new ZLine
	 * @param duration The duration of the line
	 * @param source The source of the line
	 * @param reason The reason for the line
	 * @param ipaddr The IP mask
	 * @return True if the line was added successfully
	 */
	bool add_zline(long duration, const char* source, const char* reason, const char* ipaddr);

	/** Add a new KLine
	 * @param duration The duration of the line
	 * @param source The source of the line
	 * @param reason The reason for the line
	 * @param hostmask The hostmask
	 * @return True if the line was added successfully
	 */
	bool add_kline(long duration, const char* source, const char* reason, const char* hostmask);

	/** Add a new ELine
	 * @param duration The duration of the line
	 * @param source The source of the line
	 * @param reason The reason for the line
	 * @param hostmask The hostmask
	 * @return True if the line was added successfully
	 */
	bool add_eline(long duration, const char* source, const char* reason, const char* hostmask);

	/** Delete a GLine
	 * @param hostmask The host to remove
	 * @param simulate If this is true, don't actually remove the line, just return
	 * @return True if the line was deleted successfully
	 */
	bool del_gline(const char* hostmask, bool simulate = false);

	/** Delete a QLine
	 * @param nickname The nick to remove
	 * @param simulate If this is true, don't actually remove the line, just return
	 * @return True if the line was deleted successfully
	 */
	bool del_qline(const char* nickname, bool simulate = false);

	/** Delete a ZLine
	 * @param ipaddr The IP to remove
	 * @param simulate If this is true, don't actually remove the line, just return
	 * @return True if the line was deleted successfully
	 */
	bool del_zline(const char* ipaddr, bool simulate = false);

	/** Delete a KLine
	 * @param hostmask The host to remove
	 * @param simulate If this is true, don't actually remove the line, just return
	 * @return True if the line was deleted successfully
	 */
	bool del_kline(const char* hostmask, bool simulate = false);

	/** Delete a ELine
	 * @param hostmask The host to remove
	 * @param simulate If this is true, don't actually remove the line, just return
	 * @return True if the line was deleted successfully
	 */
	bool del_eline(const char* hostmask, bool simulate = false);

	/** Check if a nickname matches a QLine
	 * @return nick The nick to check against
	 * @return The reason for the line if there is a match, or NULL if there is no match
	 */
	QLine* matches_qline(const char* nick, bool permonly = false);

	/** Check if a hostname matches a GLine
	 * @param user The user to check against
	 * @return The reason for the line if there is a match, or NULL if there is no match
	 */
	GLine* matches_gline(userrec* user, bool permonly = false);

	/** Check if a IP matches a ZLine
	 * @param ipaddr The IP to check against
	 * @return The reason for the line if there is a match, or NULL if there is no match
	 */
	ZLine* matches_zline(const char* ipaddr, bool permonly = false);

	/** Check if a hostname matches a KLine
	 * @param user The user to check against
	 * @return The reason for the line if there is a match, or NULL if there is no match
	 */
	KLine* matches_kline(userrec* user, bool permonly = false);

	/** Check if a hostname matches a ELine
	 * @param user The user to check against
	 * @return The reason for the line if there is a match, or NULL if there is no match
	 */
	ELine* matches_exception(userrec* user, bool permonly = false);

	/** Expire any pending non-permenant lines
	 */
	void expire_lines();

	/** Apply any new lines
	 * @param What The types of lines to apply, from the set
	 * APPLY_GLINES | APPLY_KLINES | APPLY_QLINES | APPLY_ZLINES | APPLY_ALL
	 * | APPLY_LOCAL_ONLY
	 */
	void apply_lines(const int What);

	/** Handle /STATS K
	 * @param user The username making the query
	 * @param results The string_list to receive the results
	 */
	void stats_k(userrec* user, string_list &results);

	/** Handle /STATS G
	 * @param user The username making the query
	 * @param results The string_list to receive the results
	 */
	void stats_g(userrec* user, string_list &results);

	/** Handle /STATS Q
	 * @param user The username making the query
	 * @param results The string_list to receive the results
	 */
	void stats_q(userrec* user, string_list &results);

	/** Handle /STATS Z
	 * @param user The username making the query
	 * @param results The string_list to receive the results
	 */
	void stats_z(userrec* user, string_list &results);

	/** Handle /STATS E
	 * @param user The username making the query
	 * @param results The string_list to receive the results
	 */
	void stats_e(userrec* user, string_list &results);

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

#endif

