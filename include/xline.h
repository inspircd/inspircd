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

class KLine : public XLine
{
	/** Hostmask (ident@host) to match against
	 * May contain wildcards.
	 */
	char hostmask[MAXBUF];
};

class GLine : public XLine
{
	/** Hostmask (ident@host) to match against
	 * May contain wildcards.
	 */
	char hostmask[MAXBUF];
};

class ZLine : public XLine
{
	/** IP Address (xx.yy.zz.aa) to match against
	 * May contain wildcards and may be CIDR
	 */
	char ipaddr[MAXBUF];
};

class QLine : public XLine
{
	/** Nickname to match against.
	 * May contain wildcards.
	 */
	char nick[MAXBUF];
};

#endif

