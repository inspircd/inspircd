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

#include "inspircd.h"
#include "inspircd_io.h"
#include "inspircd_util.h"
#include "inspircd_config.h"
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
#include "connection.h"
#include "users.h"
#include "servers.h"
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

using namespace std;

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

extern std::vector<int> fd_reap;
extern std::vector<std::string> module_names;

extern int boundPortCount;
extern int portCount;
extern int SERVERportCount;
extern int ports[MAXSOCKS];
extern int defaultRoute;

extern std::vector<long> auth_cookies;
extern std::stringstream config_f;

extern serverrec* me[32];

extern FILE *log_file;

typedef nspace::hash_map<std::string, userrec*, nspace::hash<string>, StrHashComp> user_hash;
typedef nspace::hash_map<std::string, chanrec*, nspace::hash<string>, StrHashComp> chan_hash;
typedef nspace::hash_map<in_addr,string*, nspace::hash<in_addr>, InAddr_HashComp> address_cache;
typedef nspace::hash_map<std::string, WhoWasUser*, nspace::hash<string>, StrHashComp> whowas_hash;
typedef std::deque<command_t> command_table;


extern user_hash clientlist;
extern chan_hash chanlist;
extern whowas_hash whowas;
extern command_table cmdlist;
extern file_cache MOTD;
extern file_cache RULES;
extern address_cache IP;

extern time_t TIME;

std::vector<KLine> klines;
std::vector<GLine> glines;
std::vector<ZLine> zlines;
std::vector<QLine> qlines;
std::vector<ELine> elines;

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
	glines.push_back(item);
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
        elines.push_back(item);
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
	qlines.push_back(item);
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
	zlines.push_back(item);
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
	klines.push_back(item);
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

void sync_xlines(serverrec* serv, char* tcp_host)
{
	char data[MAXBUF];
	
	// for zlines and qlines, we should first check if theyre global...
	for (std::vector<ZLine>::iterator i = zlines.begin(); i != zlines.end(); i++)
	{
		if (i->is_global)
		{
			snprintf(data,MAXBUF,"%s } %s %s %lu %lu :%s",CreateSum().c_str(),i->ipaddr,i->source,(unsigned long)i->set_time,(unsigned long)i->duration,i->reason);
			serv->SendPacket(data,tcp_host);
		}
	}
	for (std::vector<QLine>::iterator i = qlines.begin(); i != qlines.end(); i++)
	{
		if (i->is_global)
		{
			snprintf(data,MAXBUF,"%s { %s %s %lu %lu :%s",CreateSum().c_str(),i->nick,i->source,(unsigned long)i->set_time,(unsigned long)i->duration,i->reason);
			serv->SendPacket(data,tcp_host);
		}
	}
	// glines are always global, so no need to check
	for (std::vector<GLine>::iterator i = glines.begin(); i != glines.end(); i++)
	{
		snprintf(data,MAXBUF,"%s # %s %s %lu %lu :%s",CreateSum().c_str(),i->hostmask,i->source,(unsigned long)i->set_time,(unsigned long)i->duration,i->reason);
		serv->SendPacket(data,tcp_host);
	}
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
	return false;
}

// returns a pointer to the reason if a nickname matches a qline, NULL if it didnt match

char* matches_qline(const char* nick)
{
	if (qlines.empty())
		return NULL;
	for (std::vector<QLine>::iterator i = qlines.begin(); i != qlines.end(); i++)
	{
		if (match(nick,i->nick))
		{
			return i->reason;
		}
	}
	return NULL;
}

// returns a pointer to the reason if a host matches a gline, NULL if it didnt match

char* matches_gline(const char* host)
{
        if (glines.empty())
                return NULL;
	for (std::vector<GLine>::iterator i = glines.begin(); i != glines.end(); i++)
	{
		if (match(host,i->hostmask))
		{
			return i->reason;
		}
	}
	return NULL;
}

char* matches_exception(const char* host)
{
        if (elines.empty())
                return NULL;
	char host2[MAXBUF];
	snprintf(host2,MAXBUF,"*@%s",host);
        for (std::vector<ELine>::iterator i = elines.begin(); i != elines.end(); i++)
        {
                if ((match(host,i->hostmask)) || (match(host2,i->hostmask)))
                {
                        return i->reason;
                }
        }
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
	return ;	
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
	return ;	
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
	return ;	
}

// returns a pointer to the reason if an ip address matches a zline, NULL if it didnt match

char* matches_zline(const char* ipaddr)
{
        if (zlines.empty())
                return NULL;
	for (std::vector<ZLine>::iterator i = zlines.begin(); i != zlines.end(); i++)
	{
		if (match(ipaddr,i->ipaddr))
		{
			return i->reason;
		}
	}
	return NULL;
}

// returns a pointer to the reason if a host matches a kline, NULL if it didnt match

char* matches_kline(const char* host)
{
        if (klines.empty())
                return NULL;
	for (std::vector<KLine>::iterator i = klines.begin(); i != klines.end(); i++)
	{
		if (match(host,i->hostmask))
		{
			return i->reason;
		}
	}
	return NULL;
}

// removes lines that have expired

void expire_lines()
{
	bool go_again = true;
	time_t current = TIME;
	
	// because we mess up an iterator when we remove from the vector, we must bail from
	// the loop early if we delete an item, therefore this outer while loop is required.
	while (go_again)
	{
		go_again = false;

		for (std::vector<KLine>::iterator i = klines.begin(); i != klines.end(); i++)
		{
			if ((current > (i->duration + i->set_time)) && (i->duration > 0))
			{
				WriteOpers("Expiring timed K-Line %s (set by %s %d seconds ago)",i->hostmask,i->source,i->duration);
				klines.erase(i);
				go_again = true;
				break;
			}
		}

                for (std::vector<ELine>::iterator i = elines.begin(); i != elines.end(); i++)
                {
                        if ((current > (i->duration + i->set_time)) && (i->duration > 0))
                        {
                                WriteOpers("Expiring timed E-Line %s (set by %s %d seconds ago)",i->hostmask,i->source,i->duration);
                                elines.erase(i);
                                go_again = true;
                                break;
                        }
                }

		for (std::vector<GLine>::iterator i = glines.begin(); i != glines.end(); i++)
		{
			if ((current > (i->duration + i->set_time)) && (i->duration > 0))
			{
				WriteOpers("Expiring timed G-Line %s (set by %s %d seconds ago)",i->hostmask,i->source,i->duration);
				glines.erase(i);
				go_again = true;
				break;
			}
		}

		for (std::vector<ZLine>::iterator i = zlines.begin(); i != zlines.end(); i++)
		{
			if ((current > (i->duration + i->set_time)) && (i->duration > 0))
			{
				WriteOpers("Expiring timed Z-Line %s (set by %s %d seconds ago)",i->ipaddr,i->source,i->duration);
				zlines.erase(i);
				go_again = true;
				break;
			}
		}

		for (std::vector<QLine>::iterator i = qlines.begin(); i != qlines.end(); i++)
		{
			if ((current > (i->duration + i->set_time)) && (i->duration > 0))
			{
				WriteOpers("Expiring timed Q-Line %s (set by %s %d seconds ago)",i->nick,i->source,i->duration);
				qlines.erase(i);
				go_again = true;
				break;
			}
		}
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
				if (glines.size())
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
				if (klines.size())
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
				if (qlines.size())
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
				if (zlines.size())
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
	{
		WriteServ(user->fd,"216 %s :%s %d %d %s %s",user->nick,i->hostmask,i->set_time,i->duration,i->source,i->reason);
	}
}

void stats_g(userrec* user)
{
	for (std::vector<GLine>::iterator i = glines.begin(); i != glines.end(); i++)
	{
		WriteServ(user->fd,"223 %s :%s %d %d %s %s",user->nick,i->hostmask,i->set_time,i->duration,i->source,i->reason);
	}
}

void stats_q(userrec* user)
{
	for (std::vector<QLine>::iterator i = qlines.begin(); i != qlines.end(); i++)
	{
		WriteServ(user->fd,"217 %s :%s %d %d %s %s",user->nick,i->nick,i->set_time,i->duration,i->source,i->reason);
	}
}

void stats_z(userrec* user)
{
	for (std::vector<ZLine>::iterator i = zlines.begin(); i != zlines.end(); i++)
	{
		WriteServ(user->fd,"223 %s :%s %d %d %s %s",user->nick,i->ipaddr,i->set_time,i->duration,i->source,i->reason);
	}
}

void stats_e(userrec* user)
{
        for (std::vector<ELine>::iterator i = elines.begin(); i != elines.end(); i++)
        {
                WriteServ(user->fd,"223 %s :%s %d %d %s %s",user->nick,i->hostmask,i->set_time,i->duration,i->source,i->reason);
        }
}
