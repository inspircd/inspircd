/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd_config.h"
#include "inspircd.h"
#include "configreader.h"
#include "hash_map.h"
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#ifndef RUSAGE_SELF
#define   RUSAGE_SELF     0
#define   RUSAGE_CHILDREN     -1
#endif
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
#include "helperfuncs.h"
#include "hashcomp.h"
#include "socketengine.h"
#include "command_parse.h"
#include "commands/cmd_stats.h"

extern ServerConfig* Config;
extern InspIRCd* ServerInstance;
extern int MODCOUNT;
extern ModuleList modules;
extern FactoryList factory;
extern time_t TIME;
extern user_hash clientlist;
extern chan_hash chanlist;

void cmd_stats::Handle (const char** parameters, int pcnt, userrec *user)
{
	string_list values;
	DoStats(*parameters[0], user, values);
	for (size_t i = 0; i < values.size(); i++)
		Write(user->fd, ":%s", values[i].c_str());
}

void DoStats(char statschar, userrec* user, string_list &results)
{
	std::string sn = Config->ServerName;

	if ((*Config->OperOnlyStats) && (strchr(Config->OperOnlyStats,statschar)) && (!*user->oper))
	{
		results.push_back(sn+std::string(" 481 ")+user->nick+" :Permission denied - STATS "+statschar+" is oper-only");
		return;
	}
	
	int MOD_RESULT = 0;
	FOREACH_RESULT(I_OnStats,OnStats(statschar,user,results));
	if (MOD_RESULT)
		return;

	if (statschar == 'c')
	{
		/* This stats symbol must be handled by a linking module */
	}
	
	if (statschar == 'i')
	{
		int idx = 0;
		for (ClassVector::iterator i = Config->Classes.begin(); i != Config->Classes.end(); i++)
		{
			results.push_back(sn+" 215 "+user->nick+" I * * * "+ConvToStr(MAXCLIENTS)+" "+ConvToStr(idx)+" "+Config->ServerName+" *");
			idx++;
		}
	}
	
	if (statschar == 'y')
	{
		int idx = 0;
		for (ClassVector::iterator i = Config->Classes.begin(); i != Config->Classes.end(); i++)
		{
			results.push_back(sn+" 218 "+user->nick+" Y "+ConvToStr(idx)+" 120 0 "+ConvToStr(i->flood)+" "+ConvToStr(i->registration_timeout));
			idx++;
		}
	}

	if (statschar == 'U')
	{
		char ulined[MAXBUF];
		for (int i = 0; i < Config->ConfValueEnum(Config->config_data, "uline"); i++)
		{
			Config->ConfValue(Config->config_data, "uline","server", i, ulined, MAXBUF);
			results.push_back(sn+" 248 "+user->nick+" U "+std::string(ulined));
		}
	}
	
	if (statschar == 'P')
	{
		int idx = 0;
	  	for (user_hash::iterator i = clientlist.begin(); i != clientlist.end(); i++)
		{
			if (*i->second->oper)
			{
				results.push_back(sn+" 249 "+user->nick+" :"+i->second->nick+" ("+i->second->ident+"@"+i->second->dhost+") Idle: "+ConvToStr(TIME - i->second->idle_lastmsg));
				idx++;
			}
		}
		results.push_back(sn+" 249 "+user->nick+" :"+ConvToStr(idx)+" OPER(s)");
	}
 
	if (statschar == 'k')
	{
		stats_k(user,results);
	}

	if (statschar == 'g')
	{
		stats_g(user,results);
	}

	if (statschar == 'q')
	{
		stats_q(user,results);
	}

	if (statschar == 'Z')
	{
		stats_z(user,results);
	}

	if (statschar == 'e')
	{
		stats_e(user,results);
	}

	/* stats m (list number of times each command has been used, plus bytecount) */
	if (statschar == 'm')
	{
		for (nspace::hash_map<std::string,command_t*>::iterator i = ServerInstance->Parser->cmdlist.begin(); i != ServerInstance->Parser->cmdlist.end(); i++)
		{
			if (i->second->use_count)
			{
				/* RPL_STATSCOMMANDS */
				results.push_back(sn+" 212 "+user->nick+" "+i->second->command+" "+ConvToStr(i->second->use_count)+" "+ConvToStr(i->second->total_bytes));
			}
		}
			
	}

	/* stats z (debug and memory info) */
	if (statschar == 'z')
	{
		rusage R;
		results.push_back(sn+" 249 "+user->nick+" :Users(HASH_MAP) "+ConvToStr(clientlist.size())+" ("+ConvToStr(clientlist.size()*sizeof(userrec))+" bytes, "+ConvToStr(clientlist.bucket_count())+" buckets)");
		results.push_back(sn+" 249 "+user->nick+" :Channels(HASH_MAP) "+ConvToStr(chanlist.size())+" ("+ConvToStr(chanlist.size()*sizeof(chanrec))+" bytes, "+ConvToStr(chanlist.bucket_count())+" buckets)");
		results.push_back(sn+" 249 "+user->nick+" :Commands(VECTOR) "+ConvToStr(ServerInstance->Parser->cmdlist.size())+" ("+ConvToStr(ServerInstance->Parser->cmdlist.size()*sizeof(command_t))+" bytes)");
		results.push_back(sn+" 249 "+user->nick+" :MOTD(VECTOR) "+ConvToStr(Config->MOTD.size())+", RULES(VECTOR) "+ConvToStr(Config->RULES.size()));
		results.push_back(sn+" 249 "+user->nick+" :Modules(VECTOR) "+ConvToStr(modules.size())+" ("+ConvToStr(modules.size()*sizeof(Module))+")");
		results.push_back(sn+" 249 "+user->nick+" :ClassFactories(VECTOR) "+ConvToStr(factory.size())+" ("+ConvToStr(factory.size()*sizeof(ircd_module))+")");
		if (!getrusage(RUSAGE_SELF,&R))
		{
			results.push_back(sn+" 249 "+user->nick+" :Total allocation: "+ConvToStr(R.ru_maxrss)+"K");
			results.push_back(sn+" 249 "+user->nick+" :Signals:          "+ConvToStr(R.ru_nsignals));
			results.push_back(sn+" 249 "+user->nick+" :Page faults:      "+ConvToStr(R.ru_majflt));
			results.push_back(sn+" 249 "+user->nick+" :Swaps:            "+ConvToStr(R.ru_nswap));
			results.push_back(sn+" 249 "+user->nick+" :Context Switches: "+ConvToStr(R.ru_nvcsw+R.ru_nivcsw));
		}
	}

	if (statschar == 'T')
	{
		results.push_back(sn+" 249 "+user->nick+" :accepts "+ConvToStr(ServerInstance->stats->statsAccept)+" refused "+ConvToStr(ServerInstance->stats->statsRefused));
		results.push_back(sn+" 249 "+user->nick+" :unknown commands "+ConvToStr(ServerInstance->stats->statsUnknown));
		results.push_back(sn+" 249 "+user->nick+" :nick collisions "+ConvToStr(ServerInstance->stats->statsCollisions));
		results.push_back(sn+" 249 "+user->nick+" :dns requests "+ConvToStr(ServerInstance->stats->statsDns)+" succeeded "+ConvToStr(ServerInstance->stats->statsDnsGood)+" failed "+ConvToStr(ServerInstance->stats->statsDnsBad));
		results.push_back(sn+" 249 "+user->nick+" :connections "+ConvToStr(ServerInstance->stats->statsConnects));
		char buffer[MAXBUF];
		snprintf(buffer,MAXBUF," 249 %s :bytes sent %5.2fK recv %5.2fK",user->nick,ServerInstance->stats->statsSent / 1024,ServerInstance->stats->statsRecv / 1024);
		results.push_back(sn+buffer);
	}

	/* stats o */
	if (statschar == 'o')
	{
		for (int i = 0; i < Config->ConfValueEnum(Config->config_data, "oper"); i++)
		{
			char LoginName[MAXBUF];
			char HostName[MAXBUF];
			char OperType[MAXBUF];
			Config->ConfValue(Config->config_data, "oper","name", i, LoginName, MAXBUF);
			Config->ConfValue(Config->config_data, "oper","host", i, HostName, MAXBUF);
			Config->ConfValue(Config->config_data, "oper","type", i, OperType, MAXBUF);
			results.push_back(sn+" 243 "+user->nick+" O "+HostName+" * "+LoginName+" "+OperType+" 0");
		}
	}
	
	/* stats l (show user I/O stats) */
	if (statschar == 'l')
	{
		results.push_back(sn+" 211 "+user->nick+" :server:port nick bytes_in cmds_in bytes_out cmds_out");
	  	for (user_hash::iterator i = clientlist.begin(); i != clientlist.end(); i++)
		{
			if (isnick(i->second->nick))
			{
				results.push_back(sn+" 211 "+user->nick+" :"+i->second->server+":"+ConvToStr(i->second->GetPort())+" "+i->second->nick+" "+ConvToStr(i->second->bytes_in)+" "+ConvToStr(i->second->cmds_in)+" "+ConvToStr(i->second->bytes_out)+" "+ConvToStr(i->second->cmds_out));
			}
			else
			{
				results.push_back(sn+" 211 "+user->nick+" :"+i->second->server+":"+ConvToStr(i->second->GetPort())+" (unknown@"+ConvToStr(i->second->fd)+") "+ConvToStr(i->second->bytes_in)+" "+ConvToStr(i->second->cmds_in)+" "+ConvToStr(i->second->bytes_out)+" "+ConvToStr(i->second->cmds_out));
			}
			
		}
	}
	
	/* stats u (show server uptime) */
	if (statschar == 'u')
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
			char buffer[MAXBUF];
			snprintf(buffer,MAXBUF,"242 %s :Server up %d years, %d days, %.2d:%.2d:%.2d",user->nick,(stime->tm_year-70),stime->tm_yday,stime->tm_hour,stime->tm_min,stime->tm_sec);
			results.push_back(sn+buffer);
		}
		else
		{
			char buffer[MAXBUF];
			snprintf(buffer,MAXBUF,"242 %s :Server up %d days, %.2d:%.2d:%.2d",user->nick,stime->tm_yday,stime->tm_hour,stime->tm_min,stime->tm_sec);
			results.push_back(sn+buffer);
		}
	}

	results.push_back(sn+" 219 "+user->nick+" "+statschar+" :End of /STATS report");
	WriteOpers("*** Notice: %s '%c' requested by %s (%s@%s)",(!strcmp(user->server,Config->ServerName) ? "Stats" : "Remote stats"),statschar,user->nick,user->ident,user->host);

	return;
}

