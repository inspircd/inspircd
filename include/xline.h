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

/** ZLine class
 */
class ZLine : public XLine
{
  public:
	/** IP Address (xx.yy.zz.aa) to match against
	 * May contain wildcards.
	 */
	char ipaddr[MAXBUF];
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
};

void read_xline_defaults();

void add_gline(long duration, char* source, char* reason, char* hostmask);
void add_qline(long duration, char* source, char* reason, char* nickname);
void add_zline(long duration, char* source, char* reason, char* ipaddr);
void add_kline(long duration, char* source, char* reason, char* hostmask);

bool del_gline(char* hostmask);
bool del_qline(char* nickname);
bool del_zline(char* ipaddr);
bool del_kline(char* hostmask);

char* matches_qline(const char* nick);
char* matches_gline(const char* host);
char* matches_zline(const char* ipaddr);
char* matches_kline(const char* host);

void expire_lines();
void apply_lines();

void stats_k(userrec* user);
void stats_g(userrec* user);
void stats_q(userrec* user);
void stats_z(userrec* user);

void gline_set_creation_time(char* host, time_t create_time);
void qline_set_creation_time(char* nick, time_t create_time);
void zline_set_creation_time(char* ip, time_t create_time);

#endif

