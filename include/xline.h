/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *		       E-mail:
 *		<brain@chatspike.net>
 *	   	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
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
	char hostmask[200];
};

class ELine : public XLine
{
  public:
	/** Hostmask (ident@host) to match against
	 * May contain wildcards.
	 */
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

bool InitXLine(const char* tag);
bool DoneXLine(const char* tag);

bool DoZLine(const char* tag, char** entries, void** values, int* types);
bool DoQLine(const char* tag, char** entries, void** values, int* types);
bool DoKLine(const char* tag, char** entries, void** values, int* types);
bool DoELine(const char* tag, char** entries, void** values, int* types);

bool add_gline(long duration, const char* source, const char* reason, const char* hostmask);
bool add_qline(long duration, const char* source, const char* reason, const char* nickname);
bool add_zline(long duration, const char* source, const char* reason, const char* ipaddr);
bool add_kline(long duration, const char* source, const char* reason, const char* hostmask);
bool add_eline(long duration, const char* source, const char* reason, const char* hostmask);

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
void apply_lines(const int What);

void stats_k(userrec* user);
void stats_g(userrec* user);
void stats_q(userrec* user);
void stats_z(userrec* user);
void stats_e(userrec* user);

void gline_set_creation_time(char* host, time_t create_time);
void qline_set_creation_time(char* nick, time_t create_time);
void zline_set_creation_time(char* ip, time_t create_time);
void eline_set_creation_time(char* host, time_t create_time);
	
bool zline_make_global(const char* ipaddr);
bool qline_make_global(const char* nickname);

#endif
