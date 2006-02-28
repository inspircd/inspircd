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

using namespace std;

#include "inspircd_config.h"
#include "inspircd.h"
#include "inspircd_io.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <time.h>
#include <string>
#ifdef GCC3
#include <ext/hash_map>
#else
#include <hash_map>
#endif
#include <map>
#include <sstream>
#include <vector>
#include <deque>
#include "users.h"
#include "ctables.h"
#include "globals.h"
#include "modules.h"
#include "dynamic.h"
#include "wildcard.h"
#include "message.h"
#include "commands.h"
#include "xline.h"
#include "inspstring.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "typedefs.h"
#include "cull_list.h"

extern ServerConfig *Config;

extern int MODCOUNT;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
extern ServerConfig* Config;
extern user_hash clientlist;
extern std::vector<userrec*> local_users;

/* Version two, now with optimized expiry!
 *
 * Because the old way was horrendously slow, the new way of expiring xlines is very
 * very efficient. I have improved the efficiency of the algorithm in two ways:
 *
 * (1) There are now two lists of items for each linetype. One list holds temporary
 *     items, and the other list holds permenant items (ones which will expire).
 *     Items which are on the permenant list are NEVER checked at all by the
 *     expire_lines() function.
 * (2) The temporary xline lists are always kept in strict numerical order, keyed by 
 *     current time + duration. This means that the line which is due to expire the
 *     soonest is always pointed at by vector::begin(), so a simple while loop can
 *     very efficiently, very quickly and above all SAFELY pick off the first few
 *     items in the vector which need zapping.
 *
 *     -- Brain
 */



extern time_t TIME;

/* Lists for temporary lines with an expiry time */

std::vector<KLine> klines;
std::vector<GLine> glines;
std::vector<ZLine> zlines;
std::vector<QLine> qlines;
std::vector<ELine> elines;

/* Seperate lists for perm XLines that isnt checked by expiry functions */

std::vector<KLine> pklines;
std::vector<GLine> pglines;
std::vector<ZLine> pzlines;
std::vector<QLine> pqlines;
std::vector<ELine> pelines;


bool GSortComparison ( const GLine one, const GLine two );
bool ZSortComparison ( const ZLine one, const ZLine two );
bool ESortComparison ( const ELine one, const ELine two );
bool QSortComparison ( const QLine one, const QLine two );
bool KSortComparison ( const KLine one, const KLine two );

// Reads the default bans from the config file.
// only a very small number of bans are defined
// this way these days, such as qlines against 
// services nicks, etc.

void read_xline_defaults()
{
	char ipmask[MAXBUF];
	char nick[MAXBUF];
	char host[MAXBUF];
	char reason[MAXBUF];

	for (int i = 0; i < Config->ConfValueEnum("badip",&Config->config_f); i++)
	{
		Config->ConfValue("badip","ipmask",i,ipmask,&Config->config_f);
		Config->ConfValue("badip","reason",i,reason,&Config->config_f);
		add_zline(0,"<Config>",reason,ipmask);
		log(DEBUG,"Read Z line (badip tag): ipmask=%s reason=%s",ipmask,reason);
	}
	
	for (int i = 0; i < Config->ConfValueEnum("badnick",&Config->config_f); i++)
	{
		Config->ConfValue("badnick","nick",i,nick,&Config->config_f);
		Config->ConfValue("badnick","reason",i,reason,&Config->config_f);
		add_qline(0,"<Config>",reason,nick);
		log(DEBUG,"Read Q line (badnick tag): nick=%s reason=%s",nick,reason);
	}
	
	for (int i = 0; i < Config->ConfValueEnum("badhost",&Config->config_f); i++)
	{
		Config->ConfValue("badhost","host",i,host,&Config->config_f);
		Config->ConfValue("badhost","reason",i,reason,&Config->config_f);
		add_kline(0,"<Config>",reason,host);
		log(DEBUG,"Read K line (badhost tag): host=%s reason=%s",host,reason);
	}
	for (int i = 0; i < Config->ConfValueEnum("exception",&Config->config_f); i++)
	{
		Config->ConfValue("exception","host",i,host,&Config->config_f);
		Config->ConfValue("exception","reason",i,reason,&Config->config_f);
		add_eline(0,"<Config>",reason,host);
		log(DEBUG,"Read E line (exception tag): host=%s reason=%s",host,reason);
	}
}

// adds a g:line

bool add_gline(long duration, const char* source,const char* reason,const char* hostmask)
{
	bool ret = del_gline(hostmask);
	GLine item;
	item.duration = duration;
	strlcpy(item.hostmask,hostmask,199);
	strlcpy(item.reason,reason,MAXBUF);
	strlcpy(item.source,source,255);
	item.n_matches = 0;
	item.set_time = TIME;
	if (duration)
	{
		glines.push_back(item);
		sort(glines.begin(), glines.end(),GSortComparison);
	}
	else
	{
		pglines.push_back(item);
	}
	return !ret;
}

// adds an e:line (exception to bans)

bool add_eline(long duration, const char* source, const char* reason, const char* hostmask)
{
        bool ret = del_eline(hostmask);
        ELine item;
        item.duration = duration;
        strlcpy(item.hostmask,hostmask,199);
        strlcpy(item.reason,reason,MAXBUF);
        strlcpy(item.source,source,255);
        item.n_matches = 0;
        item.set_time = TIME;
	if (duration)
	{
        	elines.push_back(item);
		sort(elines.begin(), elines.end(),ESortComparison);
	}
	else
	{
		pelines.push_back(item);
	}
	return !ret;
}

// adds a q:line

bool add_qline(long duration, const char* source, const char* reason, const char* nickname)
{
	bool ret = del_qline(nickname);
	QLine item;
	item.duration = duration;
	strlcpy(item.nick,nickname,63);
	strlcpy(item.reason,reason,MAXBUF);
	strlcpy(item.source,source,255);
	item.n_matches = 0;
	item.is_global = false;
	item.set_time = TIME;
	if (duration)
	{
		qlines.push_back(item);
		sort(qlines.begin(), qlines.end(),QSortComparison);
	}
	else
	{
		pqlines.push_back(item);
	}
	return !ret;
}

// adds a z:line

bool add_zline(long duration, const char* source, const char* reason, const char* ipaddr)
{
	bool ret = del_zline(ipaddr);
	ZLine item;
	item.duration = duration;
	if (strchr(ipaddr,'@'))
	{
		while (*ipaddr != '@')
			ipaddr++;
		ipaddr++;
	}
	strlcpy(item.ipaddr,ipaddr,39);
	strlcpy(item.reason,reason,MAXBUF);
	strlcpy(item.source,source,255);
	item.n_matches = 0;
	item.is_global = false;
	item.set_time = TIME;
	if (duration)
	{
		zlines.push_back(item);
		sort(zlines.begin(), zlines.end(),ZSortComparison);
	}
	else
	{
		pzlines.push_back(item);
	}
	return !ret;
}

// adds a k:line

bool add_kline(long duration, const char* source, const char* reason, const char* hostmask)
{
	bool ret = del_kline(hostmask);
	KLine item;
	item.duration = duration;
	strlcpy(item.hostmask,hostmask,200);
	strlcpy(item.reason,reason,MAXBUF);
	strlcpy(item.source,source,255);
	item.n_matches = 0;
	item.set_time = TIME;
	if (duration)
	{
		klines.push_back(item);
		sort(klines.begin(), klines.end(),KSortComparison);
	}
	else
	{
		pklines.push_back(item);
	}
	return !ret;
}

// deletes a g:line, returns true if the line existed and was removed

bool del_gline(const char* hostmask)
{
	for (std::vector<GLine>::iterator i = glines.begin(); i != glines.end(); i++)
	{
		if (!strcasecmp(hostmask,i->hostmask))
		{
			glines.erase(i);
			return true;
		}
	}
	for (std::vector<GLine>::iterator i = pglines.begin(); i != pglines.end(); i++)
	{
		if (!strcasecmp(hostmask,i->hostmask))
		{
			pglines.erase(i);
			return true;
		}
	}
	return false;
}

// deletes a e:line, returns true if the line existed and was removed

bool del_eline(const char* hostmask)
{
        for (std::vector<ELine>::iterator i = elines.begin(); i != elines.end(); i++)
        {
                if (!strcasecmp(hostmask,i->hostmask))
                {
                        elines.erase(i);
                        return true;
                }
        }
	for (std::vector<ELine>::iterator i = pelines.begin(); i != pelines.end(); i++)
	{
		if (!strcasecmp(hostmask,i->hostmask))
		{
			pelines.erase(i);
			return true;
		}
	}
        return false;
}

// deletes a q:line, returns true if the line existed and was removed

bool del_qline(const char* nickname)
{
	for (std::vector<QLine>::iterator i = qlines.begin(); i != qlines.end(); i++)
	{
		if (!strcasecmp(nickname,i->nick))
		{
			qlines.erase(i);
			return true;
		}
	}
	for (std::vector<QLine>::iterator i = pqlines.begin(); i != pqlines.end(); i++)
	{
		if (!strcasecmp(nickname,i->nick))
		{
			pqlines.erase(i);
			return true;
		}
	}
	return false;
}

bool qline_make_global(const char* nickname)
{
	for (std::vector<QLine>::iterator i = qlines.begin(); i != qlines.end(); i++)
	{
		if (!strcasecmp(nickname,i->nick))
		{
			i->is_global = true;
			return true;
		}
	}
	return false;
}

bool zline_make_global(const char* ipaddr)
{
	for (std::vector<ZLine>::iterator i = zlines.begin(); i != zlines.end(); i++)
	{
		if (!strcasecmp(ipaddr,i->ipaddr))
		{
			i->is_global = true;
			return true;
		}
	}
	return false;
}

// deletes a z:line, returns true if the line existed and was removed

bool del_zline(const char* ipaddr)
{
	for (std::vector<ZLine>::iterator i = zlines.begin(); i != zlines.end(); i++)
	{
		if (!strcasecmp(ipaddr,i->ipaddr))
		{
			zlines.erase(i);
			return true;
		}
	}
	for (std::vector<ZLine>::iterator i = pzlines.begin(); i != pzlines.end(); i++)
	{
		if (!strcasecmp(ipaddr,i->ipaddr))
		{
			pzlines.erase(i);
			return true;
		}
	}
	return false;
}

// deletes a k:line, returns true if the line existed and was removed

bool del_kline(const char* hostmask)
{
	for (std::vector<KLine>::iterator i = klines.begin(); i != klines.end(); i++)
	{
		if (!strcasecmp(hostmask,i->hostmask))
		{
			klines.erase(i);
			return true;
		}
	}
	for (std::vector<KLine>::iterator i = pklines.begin(); i != pklines.end(); i++)
	{
		if (!strcasecmp(hostmask,i->hostmask))
		{
			pklines.erase(i);
			return true;
		}
	}
	return false;
}

// returns a pointer to the reason if a nickname matches a qline, NULL if it didnt match

char* matches_qline(const char* nick)
{
	if ((qlines.empty()) && (pqlines.empty()))
		return NULL;
	for (std::vector<QLine>::iterator i = qlines.begin(); i != qlines.end(); i++)
		if (match(nick,i->nick))
			return i->reason;
	for (std::vector<QLine>::iterator i = pqlines.begin(); i != pqlines.end(); i++)
		if (match(nick,i->nick))
			return i->reason;
	return NULL;
}

// returns a pointer to the reason if a host matches a gline, NULL if it didnt match

char* matches_gline(const char* host)
{
        if ((glines.empty()) && (pglines.empty()))
                return NULL;
	for (std::vector<GLine>::iterator i = glines.begin(); i != glines.end(); i++)
		if (match(host,i->hostmask))
			return i->reason;
	for (std::vector<GLine>::iterator i = pglines.begin(); i != pglines.end(); i++)
		if (match(host,i->hostmask))
			return i->reason;
	return NULL;
}

char* matches_exception(const char* host)
{
        if ((elines.empty()) && (pelines.empty()))
                return NULL;
	char host2[MAXBUF];
	snprintf(host2,MAXBUF,"*@%s",host);
        for (std::vector<ELine>::iterator i = elines.begin(); i != elines.end(); i++)
                if ((match(host,i->hostmask)) || (match(host2,i->hostmask)))
                        return i->reason;
	for (std::vector<ELine>::iterator i = pelines.begin(); i != pelines.end(); i++)
		if ((match(host,i->hostmask)) || (match(host2,i->hostmask)))
			return i->reason;
        return NULL;
}


void gline_set_creation_time(char* host, time_t create_time)
{
	for (std::vector<GLine>::iterator i = glines.begin(); i != glines.end(); i++)
	{
		if (!strcasecmp(host,i->hostmask))
		{
			i->set_time = create_time;
			return;
		}
	}
	for (std::vector<GLine>::iterator i = pglines.begin(); i != pglines.end(); i++)
	{
		if (!strcasecmp(host,i->hostmask))
		{
			i->set_time = create_time;
			return;
		}
	}
	return ;	
}

void eline_set_creation_time(char* host, time_t create_time)
{
	for (std::vector<ELine>::iterator i = elines.begin(); i != elines.end(); i++)
	{
		if (!strcasecmp(host,i->hostmask))
		{
			i->set_time = create_time;
			return;
		}
	}
	for (std::vector<ELine>::iterator i = pelines.begin(); i != pelines.end(); i++)	
	{
		if (!strcasecmp(host,i->hostmask))
		{
			i->set_time = create_time;
			return;
		}
	}
	return;
}

void qline_set_creation_time(char* nick, time_t create_time)
{
	for (std::vector<QLine>::iterator i = qlines.begin(); i != qlines.end(); i++)
	{
		if (!strcasecmp(nick,i->nick))
		{
			i->set_time = create_time;
			return;
		}
	}
	for (std::vector<QLine>::iterator i = pqlines.begin(); i != pqlines.end(); i++)
	{
		if (!strcasecmp(nick,i->nick))
		{
			i->set_time = create_time;
			return;
		}
	}
	return;
}

void zline_set_creation_time(char* ip, time_t create_time)
{
	for (std::vector<ZLine>::iterator i = zlines.begin(); i != zlines.end(); i++)
	{
		if (!strcasecmp(ip,i->ipaddr))
		{
			i->set_time = create_time;
			return;
		}
	}
	for (std::vector<ZLine>::iterator i = pzlines.begin(); i != pzlines.end(); i++)
	{
		if (!strcasecmp(ip,i->ipaddr))
		{
			i->set_time = create_time;
			return;
		}
	}
	return;
}

// returns a pointer to the reason if an ip address matches a zline, NULL if it didnt match

char* matches_zline(const char* ipaddr)
{
        if ((zlines.empty()) && (pzlines.empty()))
                return NULL;
	for (std::vector<ZLine>::iterator i = zlines.begin(); i != zlines.end(); i++)
		if (match(ipaddr,i->ipaddr))
			return i->reason;
	for (std::vector<ZLine>::iterator i = pzlines.begin(); i != pzlines.end(); i++)
		if (match(ipaddr,i->ipaddr))
			return i->reason;
	return NULL;
}

// returns a pointer to the reason if a host matches a kline, NULL if it didnt match

char* matches_kline(const char* host)
{
        if ((klines.empty()) && (pklines.empty()))
                return NULL;
	for (std::vector<KLine>::iterator i = klines.begin(); i != klines.end(); i++)
		if (match(host,i->hostmask))
			return i->reason;
	for (std::vector<KLine>::iterator i = pklines.begin(); i != pklines.end(); i++)
		if (match(host,i->hostmask))
			return i->reason;
	return NULL;
}

bool GSortComparison ( const GLine one, const GLine two )
{
	return (one.duration + one.set_time) < (two.duration + two.set_time);
}

bool ESortComparison ( const ELine one, const ELine two )
{
        return (one.duration + one.set_time) < (two.duration + two.set_time);
}

bool ZSortComparison ( const ZLine one, const ZLine two )
{
	return (one.duration + one.set_time) < (two.duration + two.set_time);
}

bool KSortComparison ( const KLine one, const KLine two )
{
        return (one.duration + one.set_time) < (two.duration + two.set_time);
}

bool QSortComparison ( const QLine one, const QLine two )
{
        return (one.duration + one.set_time) < (two.duration + two.set_time);
}

// removes lines that have expired

void expire_lines()
{
	time_t current = TIME;

	/* Because we now store all our XLines in sorted order using (i->duration + i->set_time) as a key, this
	 * means that to expire the XLines we just need to do a while, picking off the top few until there are
	 * none left at the head of the queue that are after the current time.
	 */

	while ((glines.size()) && (current > (glines.begin()->duration + glines.begin()->set_time)))
	{
		std::vector<GLine>::iterator i = glines.begin();
		WriteOpers("Expiring timed G-Line %s (set by %s %d seconds ago)",i->hostmask,i->source,i->duration);
		glines.erase(i);
	}

	while ((elines.size()) && (current > (elines.begin()->duration + elines.begin()->set_time)))
	{
		std::vector<ELine>::iterator i = elines.begin();
		WriteOpers("Expiring timed E-Line %s (set by %s %d seconds ago)",i->hostmask,i->source,i->duration);
		elines.erase(i);
	}

	while ((zlines.size()) && (current > (zlines.begin()->duration + zlines.begin()->set_time)))
	{
		std::vector<ZLine>::iterator i = zlines.begin();
		WriteOpers("Expiring timed Z-Line %s (set by %s %d seconds ago)",i->ipaddr,i->source,i->duration);
		zlines.erase(i);
	}

	while ((klines.size()) && (current > (klines.begin()->duration + klines.begin()->set_time)))
	{
		std::vector<KLine>::iterator i = klines.begin();
		WriteOpers("Expiring timed K-Line %s (set by %s %d seconds ago)",i->hostmask,i->source,i->duration);
		klines.erase(i);
	}

	while ((qlines.size()) && (current > (qlines.begin()->duration + qlines.begin()->set_time)))
	{
		std::vector<QLine>::iterator i = qlines.begin();
		WriteOpers("Expiring timed Q-Line %s (set by %s %d seconds ago)",i->nick,i->source,i->duration);
		qlines.erase(i);
	}
	
}

// applies lines, removing clients and changing nicks etc as applicable

void apply_lines(const int What)
{
	char reason[MAXBUF];
	char host[MAXBUF];

	if ((!glines.size()) && (!klines.size()) && (!zlines.size()) && (!qlines.size()) &&
	(!pglines.size()) && (!pklines.size()) && (!pzlines.size()) && (!pqlines.size()))
		return;

	CullList* Goners = new CullList();
	char* check = NULL;
	for (std::vector<userrec*>::const_iterator u2 = local_users.begin(); u2 != local_users.end(); u2++)
	{
		userrec* u = (userrec*)(*u2);
		u->MakeHost(host);
		if (elines.size() || pelines.size())
		{
			// ignore people matching exempts
			if (matches_exception(host))
				continue;
		}
		if ((What & APPLY_GLINES) && (glines.size() || pglines.size()))
		{
			if ((check = matches_gline(host)))
			{
				snprintf(reason,MAXBUF,"G-Lined: %s",check);
				Goners->AddItem(u,reason);
			}
		}
		if ((What & APPLY_KLINES) && (klines.size() || pklines.size()))
		{
			if ((check = matches_kline(host)))
			{
				snprintf(reason,MAXBUF,"K-Lined: %s",check);
				Goners->AddItem(u,reason);
			}
		}
		if ((What & APPLY_QLINES) && (qlines.size() || pqlines.size()))
		{
			if ((check = matches_qline(u->nick)))
			{
				snprintf(reason,MAXBUF,"Q-Lined: %s",check);
				Goners->AddItem(u,reason);
			}
		}
		if ((What & APPLY_ZLINES) && (zlines.size() || pzlines.size()))
		{
			if ((check = matches_zline((char*)inet_ntoa(u->ip4))))
			{
				snprintf(reason,MAXBUF,"Z-Lined: %s",check);
				Goners->AddItem(u,reason);
			}
		}
	}

	Goners->Apply();
	delete Goners;
}

void stats_k(userrec* user)
{
	for (std::vector<KLine>::iterator i = klines.begin(); i != klines.end(); i++)
		WriteServ(user->fd,"216 %s :%s %d %d %s %s",user->nick,i->hostmask,i->set_time,i->duration,i->source,i->reason);
	for (std::vector<KLine>::iterator i = pklines.begin(); i != pklines.end(); i++)
		WriteServ(user->fd,"216 %s :%s %d %d %s %s",user->nick,i->hostmask,i->set_time,i->duration,i->source,i->reason);
}

void stats_g(userrec* user)
{
	for (std::vector<GLine>::iterator i = glines.begin(); i != glines.end(); i++)
		WriteServ(user->fd,"223 %s :%s %d %d %s %s",user->nick,i->hostmask,i->set_time,i->duration,i->source,i->reason);
	for (std::vector<GLine>::iterator i = pglines.begin(); i != pglines.end(); i++)
		WriteServ(user->fd,"223 %s :%s %d %d %s %s",user->nick,i->hostmask,i->set_time,i->duration,i->source,i->reason);
}

void stats_q(userrec* user)
{
	for (std::vector<QLine>::iterator i = qlines.begin(); i != qlines.end(); i++)
		WriteServ(user->fd,"217 %s :%s %d %d %s %s",user->nick,i->nick,i->set_time,i->duration,i->source,i->reason);
	for (std::vector<QLine>::iterator i = pqlines.begin(); i != pqlines.end(); i++)
		WriteServ(user->fd,"217 %s :%s %d %d %s %s",user->nick,i->nick,i->set_time,i->duration,i->source,i->reason);
}

void stats_z(userrec* user)
{
	for (std::vector<ZLine>::iterator i = zlines.begin(); i != zlines.end(); i++)
		WriteServ(user->fd,"223 %s :%s %d %d %s %s",user->nick,i->ipaddr,i->set_time,i->duration,i->source,i->reason);
	for (std::vector<ZLine>::iterator i = pzlines.begin(); i != pzlines.end(); i++)
		WriteServ(user->fd,"223 %s :%s %d %d %s %s",user->nick,i->ipaddr,i->set_time,i->duration,i->source,i->reason);
}

void stats_e(userrec* user)
{
        for (std::vector<ELine>::iterator i = elines.begin(); i != elines.end(); i++)
                WriteServ(user->fd,"223 %s :%s %d %d %s %s",user->nick,i->hostmask,i->set_time,i->duration,i->source,i->reason);
	for (std::vector<ELine>::iterator i = pelines.begin(); i != pelines.end(); i++)
		WriteServ(user->fd,"223 %s :%s %d %d %s %s",user->nick,i->hostmask,i->set_time,i->duration,i->source,i->reason);
}

