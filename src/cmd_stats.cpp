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

#include "inspircd.h"
#include "configreader.h"
#include <sys/resource.h>
#include "users.h"
#include "modules.h"
#include "xline.h"
#include "commands/cmd_stats.h"



extern "C" command_t* init_command(InspIRCd* Instance)
{
	return new cmd_stats(Instance);
}

CmdResult cmd_stats::Handle (const char** parameters, int pcnt, userrec *user)
{
	if (pcnt < 2)
	{
		string_list values;
		DoStats(this->ServerInstance, *parameters[0], user, values);
		for (size_t i = 0; i < values.size(); i++)
			user->Write(":%s", values[i].c_str());
	}

	return CMD_SUCCESS;
}

void DoStats(InspIRCd* ServerInstance, char statschar, userrec* user, string_list &results)
{
	std::string sn = ServerInstance->Config->ServerName;

	if ((*ServerInstance->Config->UserStats) && (!*user->oper) && (!strchr(ServerInstance->Config->UserStats,statschar)))
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
		for (ClassVector::iterator i = ServerInstance->Config->Classes.begin(); i != ServerInstance->Config->Classes.end(); i++)
		{
			results.push_back(sn+" 215 "+user->nick+" I * * * "+ConvToStr(MAXCLIENTS)+" "+ConvToStr(idx)+" "+ServerInstance->Config->ServerName+" *");
			idx++;
		}
	}
	
	if (statschar == 'y')
	{
		int idx = 0;
		for (ClassVector::iterator i = ServerInstance->Config->Classes.begin(); i != ServerInstance->Config->Classes.end(); i++)
		{
			results.push_back(sn+" 218 "+user->nick+" Y "+ConvToStr(idx)+" 120 0 "+ConvToStr(i->flood)+" "+ConvToStr(i->registration_timeout));
			idx++;
		}
	}

	if (statschar == 'U')
	{
		char ulined[MAXBUF];
		for (int i = 0; i < ServerInstance->Config->ConfValueEnum(ServerInstance->Config->config_data, "uline"); i++)
		{
			ServerInstance->Config->ConfValue(ServerInstance->Config->config_data, "uline","server", i, ulined, MAXBUF);
			results.push_back(sn+" 248 "+user->nick+" U "+std::string(ulined));
		}
	}
	
	if (statschar == 'P')
	{
		int idx = 0;
	  	for (user_hash::iterator i = ServerInstance->clientlist.begin(); i != ServerInstance->clientlist.end(); i++)
		{
			if ((*i->second->oper) && (!ServerInstance->ULine(i->second->server)))
			{
				results.push_back(sn+" 249 "+user->nick+" :"+i->second->nick+" ("+i->second->ident+"@"+i->second->dhost+") Idle: "+ConvToStr(ServerInstance->Time() - i->second->idle_lastmsg));
				idx++;
			}
		}
		results.push_back(sn+" 249 "+user->nick+" :"+ConvToStr(idx)+" OPER(s)");
	}
 
	if (statschar == 'k')
	{
		ServerInstance->XLines->stats_k(user,results);
	}

	if (statschar == 'g')
	{
		ServerInstance->XLines->stats_g(user,results);
	}

	if (statschar == 'q')
	{
		ServerInstance->XLines->stats_q(user,results);
	}

	if (statschar == 'Z')
	{
		ServerInstance->XLines->stats_z(user,results);
	}

	if (statschar == 'e')
	{
		ServerInstance->XLines->stats_e(user,results);
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
		results.push_back(sn+" 240 "+user->nick+" :InspIRCd(CLASS) "+ConvToStr(sizeof(InspIRCd))+" bytes");
		results.push_back(sn+" 249 "+user->nick+" :Users(HASH_MAP) "+ConvToStr(ServerInstance->clientlist.size())+" ("+ConvToStr(ServerInstance->clientlist.size()*sizeof(userrec))+" bytes, "+ConvToStr(ServerInstance->clientlist.bucket_count())+" buckets)");
		results.push_back(sn+" 249 "+user->nick+" :Channels(HASH_MAP) "+ConvToStr(ServerInstance->chanlist.size())+" ("+ConvToStr(ServerInstance->chanlist.size()*sizeof(chanrec))+" bytes, "+ConvToStr(ServerInstance->chanlist.bucket_count())+" buckets)");
		results.push_back(sn+" 249 "+user->nick+" :Commands(VECTOR) "+ConvToStr(ServerInstance->Parser->cmdlist.size())+" ("+ConvToStr(ServerInstance->Parser->cmdlist.size()*sizeof(command_t))+" bytes)");
		results.push_back(sn+" 249 "+user->nick+" :MOTD(VECTOR) "+ConvToStr(ServerInstance->Config->MOTD.size())+", RULES(VECTOR) "+ConvToStr(ServerInstance->Config->RULES.size()));
		results.push_back(sn+" 249 "+user->nick+" :Modules(VECTOR) "+ConvToStr(ServerInstance->modules.size())+" ("+ConvToStr(ServerInstance->modules.size()*sizeof(Module))+")");
		results.push_back(sn+" 249 "+user->nick+" :ClassFactories(VECTOR) "+ConvToStr(ServerInstance->factory.size())+" ("+ConvToStr(ServerInstance->factory.size()*sizeof(ircd_module))+")");
		if (!getrusage(0,&R))	/* RUSAGE_SELF */
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
		results.push_back(sn+" 249 "+user->nick+" :dns requests "+ConvToStr(ServerInstance->stats->statsDnsGood+ServerInstance->stats->statsDnsBad)+" succeeded "+ConvToStr(ServerInstance->stats->statsDnsGood)+" failed "+ConvToStr(ServerInstance->stats->statsDnsBad));
		results.push_back(sn+" 249 "+user->nick+" :connection count "+ConvToStr(ServerInstance->stats->statsConnects));
		char buffer[MAXBUF];
		snprintf(buffer,MAXBUF," 249 %s :bytes sent %5.2fK recv %5.2fK",user->nick,ServerInstance->stats->statsSent / 1024,ServerInstance->stats->statsRecv / 1024);
		results.push_back(sn+buffer);
	}

	/* stats o */
	if (statschar == 'o')
	{
		for (int i = 0; i < ServerInstance->Config->ConfValueEnum(ServerInstance->Config->config_data, "oper"); i++)
		{
			char LoginName[MAXBUF];
			char HostName[MAXBUF];
			char OperType[MAXBUF];
			ServerInstance->Config->ConfValue(ServerInstance->Config->config_data, "oper","name", i, LoginName, MAXBUF);
			ServerInstance->Config->ConfValue(ServerInstance->Config->config_data, "oper","host", i, HostName, MAXBUF);
			ServerInstance->Config->ConfValue(ServerInstance->Config->config_data, "oper","type", i, OperType, MAXBUF);
			results.push_back(sn+" 243 "+user->nick+" O "+HostName+" * "+LoginName+" "+OperType+" 0");
		}
	}

	/* stats l (show user I/O stats) */
	if (statschar == 'l')
	{
		results.push_back(sn+" 211 "+user->nick+" :nick[ident@host] sendq cmds_out bytes_out cmds_in bytes_in time_open");
	  	for (std::vector<userrec*>::iterator n = ServerInstance->local_users.begin(); n != ServerInstance->local_users.end(); n++)
		{
			userrec* i = *n;
			if (ServerInstance->IsNick(i->nick))
			{
				results.push_back(sn+" 211 "+user->nick+" "+i->nick+"["+i->ident+"@"+i->dhost+"] "+ConvToStr(i->sendq.length())+" "+ConvToStr(i->cmds_out)+" "+ConvToStr(i->bytes_out)+" "+ConvToStr(i->cmds_in)+" "+ConvToStr(i->bytes_in)+" "+ConvToStr(ServerInstance->Time() - i->age));
			}
		}
	}

	/* stats L (show user I/O stats with IP addresses) */
	if (statschar == 'L')
	{
		results.push_back(sn+" 211 "+user->nick+" :nick[ident@ip] sendq cmds_out bytes_out cmds_in bytes_in time_open");
		for (std::vector<userrec*>::iterator n = ServerInstance->local_users.begin(); n != ServerInstance->local_users.end(); n++)
		{
			userrec* i = *n;
			if (ServerInstance->IsNick(i->nick))
			{
				results.push_back(sn+" 211 "+user->nick+" "+i->nick+"["+i->ident+"@"+i->GetIPString()+"] "+ConvToStr(i->sendq.length())+" "+ConvToStr(i->cmds_out)+" "+ConvToStr(i->bytes_out)+" "+ConvToStr(i->cmds_in)+" "+ConvToStr(i->bytes_in)+" "+ConvToStr(ServerInstance->Time() - i->age));
			}
		}
	}

	/* stats u (show server uptime) */
	if (statschar == 'u')
	{
		time_t current_time = 0;
		current_time = ServerInstance->Time();
		time_t server_uptime = current_time - ServerInstance->startup_time;
		struct tm* stime;
		stime = gmtime(&server_uptime);
		/* i dont know who the hell would have an ircd running for over a year nonstop, but
		 * Craig suggested this, and it seemed a good idea so in it went */
		if (stime->tm_year > 70)
		{
			char buffer[MAXBUF];
			snprintf(buffer,MAXBUF," 242 %s :Server up %d years, %d days, %.2d:%.2d:%.2d",user->nick,(stime->tm_year-70),stime->tm_yday,stime->tm_hour,stime->tm_min,stime->tm_sec);
			results.push_back(sn+buffer);
		}
		else
		{
			char buffer[MAXBUF];
			snprintf(buffer,MAXBUF," 242 %s :Server up %d days, %.2d:%.2d:%.2d",user->nick,stime->tm_yday,stime->tm_hour,stime->tm_min,stime->tm_sec);
			results.push_back(sn+buffer);
		}
	}

	results.push_back(sn+" 219 "+user->nick+" "+statschar+" :End of /STATS report");
	ServerInstance->SNO->WriteToSnoMask('t',"%s '%c' requested by %s (%s@%s)",(!strcmp(user->server,ServerInstance->Config->ServerName) ? "Stats" : "Remote stats"),statschar,user->nick,user->ident,user->host);

	return;
}

