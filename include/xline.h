/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
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

const int APPLY_GLINES	= 1;
const int APPLY_KLINES	= 2;
const int APPLY_QLINES	= 4;
const int APPLY_ZLINES	= 8;
const int APPLY_ALL	= APPLY_GLINES | APPLY_KLINES | APPLY_QLINES | APPLY_ZLINES;

/** XLine is the base class for ban lines such as G lines and K lines.
 */
class XLine : public classbase
{
  public:

	/** The time the line was added.
	 */
	time_t set_time;
	
	/** The duration of the ban, or 0 if permenant
	 */
	long duration;
	
	/** Source of the ban. This can be a servername or an oper nickname
	 */
	char source[256];
	
	/** Reason for the ban
	 */
	char reason[MAXBUF];
	
	/** Number of times the core matches the ban, for statistics
	 */
	long n_matches;
	
};

/** KLine class
 */
class KLine : public XLine
{
  public:
	/** Hostmask (ident@host) to match against
	 * May contain wildcards.
	 */
	char identmask[20];
	char hostmask[200];
};

/** GLine class
 */
class GLine : public XLine
{
  public:
	/** Hostmask (ident@host) to match against
	 * May contain wildcards.
	 */
	char identmask[20];
	char hostmask[200];
};

/** ELine class
 */
class ELine : public XLine
{
  public:
        /** Hostmask (ident@host) to match against
         * May contain wildcards.
         */
	char identmask[20];
        char hostmask[200];
};

/** ZLine class
 */
class ZLine : public XLine
{
  public:
	/** IP Address (xx.yy.zz.aa) to match against
	 * May contain wildcards.
	 */
	char ipaddr[40];
	/** Set if this is a global Z:line
	 * (e.g. it came from another server)
	 */
	bool is_global;
};

/** QLine class
 */
class QLine : public XLine
{
  public:
	/** Nickname to match against.
	 * May contain wildcards.
	 */
	char nick[64];
	/** Set if this is a global Z:line
	 * (e.g. it came from another server)
	 */
	bool is_global;
};

class ServerConfig;
class InspIRCd;

bool InitXLine(ServerConfig* conf, const char* tag);
bool DoneXLine(ServerConfig* conf, const char* tag);

bool DoZLine(ServerConfig* conf, const char* tag, char** entries, void** values, int* types);
bool DoQLine(ServerConfig* conf, const char* tag, char** entries, void** values, int* types);
bool DoKLine(ServerConfig* conf, const char* tag, char** entries, void** values, int* types);
bool DoELine(ServerConfig* conf, const char* tag, char** entries, void** values, int* types);

typedef std::pair<std::string, std::string> IdentHostPair;

/** XLineManager is a class used to manage glines, klines, elines, zlines and qlines.
 */
class XLineManager
{
 protected:
	/** The owner/creator of this class
	 */
	InspIRCd* ServerInstance;

	/** This functor is used by the std::sort() function to keep glines in order
	 */
	static bool GSortComparison ( const GLine one, const GLine two );

	/** This functor is used by the std::sort() function to keep elines in order
	 */
	static bool ESortComparison ( const ELine one, const ELine two );

	/** This functor is used by the std::sort() function to keep zlines in order
	 */
	static bool ZSortComparison ( const ZLine one, const ZLine two );

	/** This functor is used by the std::sort() function to keep klines in order
	 */
	static bool KSortComparison ( const KLine one, const KLine two );

	/** This functor is used by the std::sort() function to keep qlines in order
	 */
	static bool QSortComparison ( const QLine one, const QLine two );
 public:
	/* Lists for temporary lines with an expiry time */

	/** Temporary KLines */
	std::vector<KLine> klines;

	/** Temporary Glines */
	std::vector<GLine> glines;

	/** Temporary Zlines */
	std::vector<ZLine> zlines;

	/** Temporary QLines */
	std::vector<QLine> qlines;

	/** Temporary ELines */
	std::vector<ELine> elines;

	/* Seperate lists for perm XLines that isnt checked by expiry functions */

	/** Permenant KLines */
	std::vector<KLine> pklines;

	/** Permenant GLines */
	std::vector<GLine> pglines;

	/** Permenant ZLines */
	std::vector<ZLine> pzlines;

	/** Permenant QLines */
	std::vector<QLine> pqlines;

	/** Permenant ELines */
	std::vector<ELine> pelines;
	
	/** Constructor
	 * @param Instance A pointer to the creator object
	 */
	XLineManager(InspIRCd* Instance);

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
	 * @return hostmask The host to remove
	 * @return True if the line was deleted successfully
	 */
	bool del_gline(const char* hostmask);

	/** Delete a QLine
	 * @return nickname The nick to remove
	 * @return True if the line was deleted successfully
	 */
	bool del_qline(const char* nickname);

	/** Delete a ZLine
	 * @return ipaddr The IP to remove
	 * @return True if the line was deleted successfully
	 */
	bool del_zline(const char* ipaddr);

	/** Delete a KLine
	 * @return hostmask The host to remove
	 * @return True if the line was deleted successfully
	 */
	bool del_kline(const char* hostmask);

	/** Delete a ELine
	 * @return hostmask The host to remove
	 * @return True if the line was deleted successfully
	 */
	bool del_eline(const char* hostmask);

	/** Check if a nickname matches a QLine
	 * @return nick The nick to check against
	 * @return The reason for the line if there is a match, or NULL if there is no match
	 */
	char* matches_qline(const char* nick);

	/** Check if a hostname matches a GLine
	 * @param user The user to check against
	 * @return The reason for the line if there is a match, or NULL if there is no match
	 */
	char* matches_gline(userrec* user);

	/** Check if a IP matches a ZLine
	 * @param ipaddr The IP to check against
	 * @return The reason for the line if there is a match, or NULL if there is no match
	 */
	char* matches_zline(const char* ipaddr);

	/** Check if a hostname matches a KLine
	 * @param user The user to check against
	 * @return The reason for the line if there is a match, or NULL if there is no match
	 */
	char* matches_kline(userrec* user);

	/** Check if a hostname matches a ELine
	 * @param user The user to check against
	 * @return The reason for the line if there is a match, or NULL if there is no match
	 */
	char* matches_exception(userrec* user);

	/** Expire any pending non-permenant lines
	 */
	void expire_lines();

	/** Apply any new lines
	 * @param What The types of lines to apply, from the set
	 * APPLY_GLINES | APPLY_KLINES | APPLY_QLINES | APPLY_ZLINES | APPLY_ALL
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
	
	/** Make a ZLine global
	 * @param ipaddr The zline to change
	 * @return True if the zline was updated
	 */
	bool zline_make_global(const char* ipaddr);

	/** Make a QLine global
	 * @param nickname The qline to change
	 * @return True if the qline was updated
	 */
	bool qline_make_global(const char* nickname);
};

#endif
