/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
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

#include <typeinfo>
#include <iostream>
#include <string>
#include <deque>
#include <sstream>
#include <vector>
#include "users.h"
#include "channels.h"


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
	char source[MAXBUF];
	
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
	char hostmask[MAXBUF];
};

/** GLine class
 */
class GLine : public XLine
{
  public:
	/** Hostmask (ident@host) to match against
	 * May contain wildcards.
	 */
	char hostmask[MAXBUF];
};

class ELine : public XLine
{
  public:
        /** Hostmask (ident@host) to match against
         * May contain wildcards.
         */
        char hostmask[MAXBUF];
};

/** ZLine class
 */
class ZLine : public XLine
{
  public:
	/** IP Address (xx.yy.zz.aa) to match against
	 * May contain wildcards.
	 */
	char ipaddr[MAXBUF];
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
	char nick[MAXBUF];
	/** Set if this is a global Z:line
	 * (e.g. it came from another server)
	 */
	bool is_global;
};

void read_xline_defaults();

void add_gline(long duration, const char* source, const char* reason, const char* hostmask);
void add_qline(long duration, const char* source, const char* reason, const char* nickname);
void add_zline(long duration, const char* source, const char* reason, const char* ipaddr);
void add_kline(long duration, const char* source, const char* reason, const char* hostmask);
void add_eline(long duration, const char* source, const char* reason, const char* hostmask);

bool del_gline(const char* hostmask);
bool del_qline(const char* nickname);
bool del_zline(const char* ipaddr);
bool del_kline(const char* hostmask);
bool del_eline(const char* hostmask);

char* matches_qline(const char* nick);
char* matches_gline(const char* host);
char* matches_zline(const char* ipaddr);
char* matches_kline(const char* host);
char* matches_exception(const char* host);

void expire_lines();
void apply_lines();

void stats_k(userrec* user);
void stats_g(userrec* user);
void stats_q(userrec* user);
void stats_z(userrec* user);
void stats_e(userrec* user);

void gline_set_creation_time(char* host, time_t create_time);
void qline_set_creation_time(char* nick, time_t create_time);
void zline_set_creation_time(char* ip, time_t create_time);

bool zline_make_global(const char* ipaddr);
bool qline_make_global(const char* nickname);

void sync_xlines(serverrec* serv, char* tcp_host);

#endif
