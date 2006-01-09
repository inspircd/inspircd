/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2005 ChatSpike-Dev.
 *                       E-mail:
 *                <brain.net>
 *                <Craig.net>
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
#include <time.h>
#include <string>
#ifdef GCC3
#include <ext/hash_map>
#else
#include <hash_map>
#endif
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#ifdef THREADED_DNS
#include <pthread.h>
#endif
#ifndef RUSAGE_SELF
#define   RUSAGE_SELF     0
#define   RUSAGE_CHILDREN     -1
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
#include "mode.h"
#include "xline.h"
#include "inspstring.h"
#include "dnsqueue.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "socketengine.h"
#include "typedefs.h"
#include "command_parse.h"
#include "cmd_stats.h"

extern ServerConfig* Config;
extern InspIRCd* ServerInstance;
extern int MODCOUNT;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
extern time_t TIME;
extern user_hash clientlist;
extern chan_hash chanlist;
extern whowas_hash whowas;
extern std::vector<userrec*> all_opers;
extern std::vector<userrec*> local_users;
extern userrec* fd_ref_table[65536];

void cmd_stats::Handle (char **parameters, int pcnt, userrec *user)
{
	if (pcnt != 1)
	{
		return;
	}
	if (strlen(parameters[0])>1)
	{
		/* make the stats query 1 character long */
		parameters[0][1] = '\0';
	}

	if ((strchr(Config->OperOnlyStats,*parameters[0])) && (!*user->oper))
	{
		WriteServ(user->fd,"481 %s :Permission denied - This stats character is set as oper-only");
		return;
	}


	int MOD_RESULT = 0;
	FOREACH_RESULT(I_OnStats,OnStats(*parameters[0],user));
	if (MOD_RESULT)
		return;

	if (*parameters[0] == 'c')
	{
		/* This stats symbol must be handled by a linking module */
	}
	
	if (*parameters[0] == 'i')
	{
		int idx = 0;
		for (ClassVector::iterator i = Config->Classes.begin(); i != Config->Classes.end(); i++)
		{
			WriteServ(user->fd,"215 %s I * * * %d %d %s *",user->nick,MAXCLIENTS,idx,Config->ServerName);
			idx++;
		}
	}
	
	if (*parameters[0] == 'y')
	{
		int idx = 0;
		for (ClassVector::iterator i = Config->Classes.begin(); i != Config->Classes.end(); i++)
		{
			WriteServ(user->fd,"218 %s Y %d %d 0 %d %d",user->nick,idx,120,i->flood,i->registration_timeout);
			idx++;
		}
	}

	if (*parameters[0] == 'U')
	{
		char ulined[MAXBUF];
		for (int i = 0; i < Config->ConfValueEnum("uline",&Config->config_f); i++)
		{
			Config->ConfValue("uline","server",i,ulined,&Config->config_f);
			WriteServ(user->fd,"248 %s U %s",user->nick,ulined);
		}
	}
	
	if (*parameters[0] == 'P')
	{
		int idx = 0;
	  	for (user_hash::iterator i = clientlist.begin(); i != clientlist.end(); i++)
		{
			if (strchr(i->second->modes,'o'))
			{
				WriteServ(user->fd,"249 %s :%s (%s@%s) Idle: %d",user->nick,i->second->nick,i->second->ident,i->second->dhost,(TIME - i->second->idle_lastmsg));
				idx++;
			}
		}
		WriteServ(user->fd,"249 %s :%d OPER(s)",user->nick,idx);
	}
 	
	if (*parameters[0] == 'k')
	{
		stats_k(user);
	}

	if (*parameters[0] == 'g')
	{
		stats_g(user);
	}

	if (*parameters[0] == 'q')
	{
		stats_q(user);
	}

	if (*parameters[0] == 'Z')
	{
		stats_z(user);
	}

	if (*parameters[0] == 'e')
	{
		stats_e(user);
	}

	/* stats m (list number of times each command has been used, plus bytecount) */
	if (*parameters[0] == 'm')
	{
		for (nspace::hash_map<std::string,command_t*>::iterator i = ServerInstance->Parser->cmdlist.begin(); i != ServerInstance->Parser->cmdlist.end(); i++)
		{
			if (i->second->use_count)
			{
				/* RPL_STATSCOMMANDS */
				WriteServ(user->fd,"212 %s %s %d %d",user->nick,i->second->command.c_str(),i->second->use_count,i->second->total_bytes);
			}
		}
			
	}

	/* stats z (debug and memory info) */
	if (*parameters[0] == 'z')
	{
		rusage R;
		WriteServ(user->fd,"249 %s :Users(HASH_MAP) %d (%d bytes, %d buckets)",user->nick,clientlist.size(),clientlist.size()*sizeof(userrec),clientlist.bucket_count());
		WriteServ(user->fd,"249 %s :Channels(HASH_MAP) %d (%d bytes, %d buckets)",user->nick,chanlist.size(),chanlist.size()*sizeof(chanrec),chanlist.bucket_count());
		WriteServ(user->fd,"249 %s :Commands(VECTOR) %d (%d bytes)",user->nick,ServerInstance->Parser->cmdlist.size(),ServerInstance->Parser->cmdlist.size()*sizeof(command_t));
		WriteServ(user->fd,"249 %s :MOTD(VECTOR) %d, RULES(VECTOR) %d",user->nick,Config->MOTD.size(),Config->RULES.size());
		WriteServ(user->fd,"249 %s :Modules(VECTOR) %d (%d)",user->nick,modules.size(),modules.size()*sizeof(Module));
		WriteServ(user->fd,"249 %s :ClassFactories(VECTOR) %d (%d)",user->nick,factory.size(),factory.size()*sizeof(ircd_module));
		if (!getrusage(RUSAGE_SELF,&R))
		{
			WriteServ(user->fd,"249 %s :Total allocation: %luK (0x%lx)",user->nick,R.ru_maxrss,R.ru_maxrss);
			WriteServ(user->fd,"249 %s :Signals:          %lu  (0x%lx)",user->nick,R.ru_nsignals,R.ru_nsignals);
			WriteServ(user->fd,"249 %s :Page faults:      %lu  (0x%lx)",user->nick,R.ru_majflt,R.ru_majflt);
			WriteServ(user->fd,"249 %s :Swaps:            %lu  (0x%lx)",user->nick,R.ru_nswap,R.ru_nswap);
			WriteServ(user->fd,"249 %s :Context Switches: %lu  (0x%lx)",user->nick,R.ru_nvcsw+R.ru_nivcsw,R.ru_nvcsw+R.ru_nivcsw);
		}
	}

	if (*parameters[0] == 'T')
	{
		WriteServ(user->fd,"249 Brain :accepts %d refused %d",ServerInstance->stats->statsAccept,ServerInstance->stats->statsRefused);
		WriteServ(user->fd,"249 Brain :unknown commands %d",ServerInstance->stats->statsUnknown);
		WriteServ(user->fd,"249 Brain :nick collisions %d",ServerInstance->stats->statsCollisions);
		WriteServ(user->fd,"249 Brain :dns requests %d succeeded %d failed %d",ServerInstance->stats->statsDns,ServerInstance->stats->statsDnsGood,ServerInstance->stats->statsDnsBad);
		WriteServ(user->fd,"249 Brain :connections %d",ServerInstance->stats->statsConnects);
		WriteServ(user->fd,"249 Brain :bytes sent %dK recv %dK",(ServerInstance->stats->statsSent / 1024),(ServerInstance->stats->statsRecv / 1024));
	}
	
	/* stats o */
	if (*parameters[0] == 'o')
	{
		for (int i = 0; i < Config->ConfValueEnum("oper",&Config->config_f); i++)
		{
			char LoginName[MAXBUF];
			char HostName[MAXBUF];
			char OperType[MAXBUF];
			Config->ConfValue("oper","name",i,LoginName,&Config->config_f);
			Config->ConfValue("oper","host",i,HostName,&Config->config_f);
			Config->ConfValue("oper","type",i,OperType,&Config->config_f);
			WriteServ(user->fd,"243 %s O %s * %s %s 0",user->nick,HostName,LoginName,OperType);
		}
	}
	
	/* stats l (show user I/O stats) */
	if (*parameters[0] == 'l')
	{
		WriteServ(user->fd,"211 %s :server:port nick bytes_in cmds_in bytes_out cmds_out",user->nick);
	  	for (user_hash::iterator i = clientlist.begin(); i != clientlist.end(); i++)
		{
			if (isnick(i->second->nick))
			{
				WriteServ(user->fd,"211 %s :%s:%d %s %d %d %d %d",user->nick,i->second->server,i->second->port,i->second->nick,i->second->bytes_in,i->second->cmds_in,i->second->bytes_out,i->second->cmds_out);
			}
			else
			{
				WriteServ(user->fd,"211 %s :%s:%d (unknown@%d) %d %d %d %d",user->nick,i->second->server,i->second->port,i->second->fd,i->second->bytes_in,i->second->cmds_in,i->second->bytes_out,i->second->cmds_out);
			}
			
		}
	}
	
	/* stats u (show server uptime) */
	if (*parameters[0] == 'u')
	{
		time_t current_time = 0;
		current_time = TIME;
		time_t server_uptime = current_time - ServerInstance->startup_time;
		struct tm* stime;
		stime = gmtime(&server_uptime);
		/* i dont know who the hell would have an ircd running for over a year nonstop, but
		 * Craig suggested this, and it seemed a good idea so in it went */
		if (stime->tm_year > 70)
		{
			WriteServ(user->fd,"242 %s :Server up %d years, %d days, %.2d:%.2d:%.2d",user->nick,(stime->tm_year-70),stime->tm_yday,stime->tm_hour,stime->tm_min,stime->tm_sec);
		}
		else
		{
			WriteServ(user->fd,"242 %s :Server up %d days, %.2d:%.2d:%.2d",user->nick,stime->tm_yday,stime->tm_hour,stime->tm_min,stime->tm_sec);
		}
	}

	WriteServ(user->fd,"219 %s %s :End of /STATS report",user->nick,parameters[0]);
	WriteOpers("*** Notice: Stats '%s' requested by %s (%s@%s)",parameters[0],user->nick,user->ident,user->host);
	
}



