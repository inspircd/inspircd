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

using namespace std;

#include "inspircd_config.h"
#include "inspircd.h"
#include "inspircd_io.h"
#include "inspircd_util.h"
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

extern int MODCOUNT;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;

extern int LogLevel;
extern char ServerName[MAXBUF];
extern char Network[MAXBUF];
extern char ServerDesc[MAXBUF];
extern char AdminName[MAXBUF];
extern char AdminEmail[MAXBUF];
extern char AdminNick[MAXBUF];
extern char diepass[MAXBUF];
extern char restartpass[MAXBUF];
extern char motd[MAXBUF];
extern char rules[MAXBUF];
extern char list[MAXBUF];
extern char PrefixQuit[MAXBUF];
extern char DieValue[MAXBUF];

extern int debugging;
extern int WHOWAS_STALE;
extern int WHOWAS_MAX;
extern int DieDelay;
extern time_t startup_time;
extern int NetBufferSize;
extern time_t nb_start;

extern std::vector<std::string> module_names;

extern int boundPortCount;
extern int portCount;

extern int ports[MAXSOCKS];

extern std::stringstream config_f;

extern FILE *log_file;

typedef nspace::hash_map<std::string, userrec*, nspace::hash<string>, irc::StrHashComp> user_hash;
typedef nspace::hash_map<std::string, chanrec*, nspace::hash<string>, irc::StrHashComp> chan_hash;
typedef nspace::hash_map<in_addr,string*, nspace::hash<in_addr>, irc::InAddr_HashComp> address_cache;
typedef nspace::hash_map<std::string, WhoWasUser*, nspace::hash<string>, irc::StrHashComp> whowas_hash;
typedef std::deque<command_t> command_table;


extern user_hash clientlist;
extern chan_hash chanlist;
extern whowas_hash whowas;
extern command_table cmdlist;
extern file_cache MOTD;
extern file_cache RULES;
extern address_cache IP;

/* Version two, now with optimized expiry!
 *
 * Because the old way was horrendously slow, the new way of expiring xlines is very
 * very efficient. I have improved the efficiency of the algorithm in two ways:
 *
 * (1) There are now two lists of items for each linetype. One list holds permenant
 *     items, and the other list holds temporary items (ones which will expire).
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

	for (int i = 0; i < ConfValueEnum("badip",&config_f); i++)
	{
		ConfValue("badip","ipmask",i,ipmask,&config_f);
		ConfValue("badip","reason",i,reason,&config_f);
		add_zline(0,"<Config>",reason,ipmask);
		log(DEBUG,"Read Z line (badip tag): ipmask=%s reason=%s",ipmask,reason);
	}
	
	for (int i = 0; i < ConfValueEnum("badnick",&config_f); i++)
	{
		ConfValue("badnick","nick",i,nick,&config_f);
		ConfValue("badnick","reason",i,reason,&config_f);
		add_qline(0,"<Config>",reason,nick);
		log(DEBUG,"Read Q line (badnick tag): nick=%s reason=%s",nick,reason);
	}
	
	for (int i = 0; i < ConfValueEnum("badhost",&config_f); i++)
	{
		ConfValue("badhost","host",i,host,&config_f);
		ConfValue("badhost","reason",i,reason,&config_f);
		add_kline(0,"<Config>",reason,host);
		log(DEBUG,"Read K line (badhost tag): host=%s reason=%s",host,reason);
	}
	for (int i = 0; i < ConfValueEnum("exception",&config_f); i++)
	{
		ConfValue("exception","host",i,host,&config_f);
		ConfValue("exception","reason",i,reason,&config_f);
		add_eline(0,"<Config>",reason,host);
		log(DEBUG,"Read E line (exception tag): host=%s reason=%s",host,reason);
	}
}

// adds a g:line

void add_gline(long duration, const char* source,const char* reason,const char* hostmask)
{
	del_gline(hostmask);
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
}

// adds an e:line (exception to bans)

void add_eline(long duration, const char* source, const char* reason, const char* hostmask)
{
        del_eline(hostmask);
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
}

// adds a q:line

void add_qline(long duration, const char* source, const char* reason, const char* nickname)
{
	del_qline(nickname);
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
}

// adds a z:line

void add_zline(long duration, const char* source, const char* reason, const char* ipaddr)
{
	del_zline(ipaddr);
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
}

// adds a k:line

void add_kline(long duration, const char* source, const char* reason, const char* hostmask)
{
	del_kline(hostmask);
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
	if (qlines.empty())
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
        if (glines.empty())
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
        if (elines.empty())
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
        if (zlines.empty())
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
        if (klines.empty())
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

void apply_lines()
{
	bool go_again = true;
	char reason[MAXBUF];
	char host[MAXBUF];
	
	if ((!glines.size()) && (!klines.size()) && (!zlines.size()) && (!qlines.size()))
		return;
	
	while (go_again)
	{
		go_again = false;
		for (user_hash::const_iterator u = clientlist.begin(); u != clientlist.end(); u++)
		{
			if (!strcasecmp(u->second->server,ServerName))
			{
				snprintf(host,MAXBUF,"%s@%s",u->second->ident,u->second->host);
				if (elines.size())
				{
					// ignore people matching exempts
					if (matches_exception(host))
						continue;
				}
				if (glines.size() || pglines.size())
				{
					char* check = matches_gline(host);
					if (check)
					{
						WriteOpers("*** User %s matches G-Line: %s",u->second->registered == 7 ? u->second->nick:"<unknown>",check);
						snprintf(reason,MAXBUF,"G-Lined: %s",check);
						kill_link(u->second,reason);
						go_again = true;
						break;
					}
				}
				if (klines.size() || pklines.size())
				{
					char* check = matches_kline(host);
					if (check)
					{
						WriteOpers("*** User %s matches K-Line: %s",u->second->registered == 7 ? u->second->nick:"<unknown>",check);
						snprintf(reason,MAXBUF,"K-Lined: %s",check);
						kill_link(u->second,reason);
						go_again = true;
						break;
					}
				}
				if (qlines.size() || pqlines.size())
				{
					char* check = matches_qline(u->second->nick);
					if (check)
					{
						snprintf(reason,MAXBUF,"Matched Q-Lined nick: %s",check);
						WriteOpers("*** Q-Lined nickname %s from %s: %s",u->second->registered == 7 ? u->second->nick:"<unknown>",u->second->host,check);
						kill_link(u->second,reason);
						go_again = true;
						break;
					}
				}
				if (zlines.size() || pzlines.size())
				{
					char* check = matches_zline(u->second->ip);
					if (check)
					{
						snprintf(reason,MAXBUF,"Z-Lined: %s",check);
						WriteOpers("*** User %s matches Z-Line: %s",u->second->registered == 7 ? u->second->nick:"<unknown>",u->second->host,check);
						kill_link(u->second,reason);
						go_again = true;
						break;
					}
				}
			}
		}
	}
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
