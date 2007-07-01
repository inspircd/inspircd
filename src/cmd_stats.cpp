/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "configreader.h"
#ifndef WIN32
#include <sys/resource.h>

/* This is just to be completely certain that the change which fixed getrusage on RH7 doesn't break anything else -- Om */
#ifndef RUSAGE_SELF
#define RUSAGE_SELF 0
#endif

#endif
#include "users.h"
#include "modules.h"
#include "xline.h"
#include "commands/cmd_stats.h"
#include "commands/cmd_whowas.h"


extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_stats(Instance);
}

CmdResult cmd_stats::Handle (const char** parameters, int pcnt, userrec *user)
{
	if (IS_LOCAL(user))
	{
		string_list values;
		DoStats(this->ServerInstance, *parameters[0], user, values);
		for (size_t i = 0; i < values.size(); i++)
			user->Write(":%s", values[i].c_str());
	}

	return CMD_SUCCESS;
}

DllExport void DoStats(InspIRCd* ServerInstance, char statschar, userrec* user, string_list &results)
{
	std::string sn = ServerInstance->Config->ServerName;

	if ((*ServerInstance->Config->UserStats) && !IS_OPER(user) && !strchr(ServerInstance->Config->UserStats,statschar))
	{
		results.push_back(sn+std::string(" 481 ")+user->nick+" :Permission denied - STATS "+statschar+" is oper-only");
		return;
	}
	
	int MOD_RESULT = 0;
	FOREACH_RESULT(I_OnStats,OnStats(statschar,user,results));
	if (MOD_RESULT)
		return;

	switch (statschar)
	{
		/* stats p (show listening ports and registered clients on each) */
		case 'p':
		{
			for (size_t i = 0; i < ServerInstance->Config->ports.size(); i++)
			{
				std::string ip = ServerInstance->Config->ports[i]->GetIP();
				if (ip.empty())
					ip.assign("*");

				results.push_back(sn+" 249 "+user->nick+" :"+ ip + ":"+ConvToStr(ServerInstance->Config->ports[i]->GetPort())+" (client, " +
						ServerInstance->Config->ports[i]->GetDescription() + ")");
			}
		}
		break;

		case 'n':
		case 'c':
		{
			/* This stats symbol must be handled by a linking module */
		}
		break;
	
		case 'i':
		{
			int idx = 0;
			for (ClassVector::iterator i = ServerInstance->Config->Classes.begin(); i != ServerInstance->Config->Classes.end(); i++)
			{
				results.push_back(sn+" 215 "+user->nick+" I NOMATCH * "+i->GetHost()+" "+ConvToStr(MAXCLIENTS)+" "+ConvToStr(idx)+" "+ServerInstance->Config->ServerName+" *");
				idx++;
			}
		}
		break;
	
		case 'Y':
		{
			int idx = 0;
			for (ClassVector::iterator i = ServerInstance->Config->Classes.begin(); i != ServerInstance->Config->Classes.end(); i++)
			{
				results.push_back(sn+" 218 "+user->nick+" Y "+ConvToStr(idx)+" "+ConvToStr(i->GetPingTime())+" 0 "+ConvToStr(i->GetSendqMax())+" :"+
						ConvToStr(i->GetFlood())+" "+ConvToStr(i->GetRegTimeout()));
				idx++;
			}
		}
		break;

		case 'U':
		{
			char ulined[MAXBUF];
			for (int i = 0; i < ServerInstance->Config->ConfValueEnum(ServerInstance->Config->config_data, "uline"); i++)
			{
				ServerInstance->Config->ConfValue(ServerInstance->Config->config_data, "uline","server", i, ulined, MAXBUF);
					results.push_back(sn+" 248 "+user->nick+" U "+std::string(ulined));
			}
		}
		break;
	
		case 'P':
		{
			int idx = 0;
		  	for (user_hash::iterator i = ServerInstance->clientlist->begin(); i != ServerInstance->clientlist->end(); i++)
			{
				if (IS_OPER(i->second) && !ServerInstance->ULine(i->second->server))
				{
					results.push_back(sn+" 249 "+user->nick+" :"+i->second->nick+" ("+i->second->ident+"@"+i->second->dhost+") Idle: "+
							(IS_LOCAL(i->second) ? ConvToStr(ServerInstance->Time() - i->second->idle_lastmsg) + " secs" : "unavailable"));
					idx++;
				}
			}
			results.push_back(sn+" 249 "+user->nick+" :"+ConvToStr(idx)+" OPER(s)");
		}
		break;
 
		case 'k':
			ServerInstance->XLines->stats_k(user,results);
		break;

		case 'g':
			ServerInstance->XLines->stats_g(user,results);
		break;

		case 'q':
			ServerInstance->XLines->stats_q(user,results);
		break;

		case 'Z':
			ServerInstance->XLines->stats_z(user,results);
		break;

		case 'e':
			ServerInstance->XLines->stats_e(user,results);
		break;

		/* stats m (list number of times each command has been used, plus bytecount) */
		case 'm':
			for (command_table::iterator i = ServerInstance->Parser->cmdlist.begin(); i != ServerInstance->Parser->cmdlist.end(); i++)
			{
				if (i->second->use_count)
				{
					/* RPL_STATSCOMMANDS */
					results.push_back(sn+" 212 "+user->nick+" "+i->second->command+" "+ConvToStr(i->second->use_count)+" "+ConvToStr(i->second->total_bytes));
				}
			}
		break;

		/* stats z (debug and memory info) */
		case 'z':
		{
			results.push_back(sn+" 240 "+user->nick+" :InspIRCd(CLASS) "+ConvToStr(sizeof(InspIRCd))+" bytes");
			results.push_back(sn+" 249 "+user->nick+" :Users(HASH_MAP) "+ConvToStr(ServerInstance->clientlist->size())+" ("+ConvToStr(ServerInstance->clientlist->size()*sizeof(userrec))+" bytes)");
			results.push_back(sn+" 249 "+user->nick+" :Channels(HASH_MAP) "+ConvToStr(ServerInstance->chanlist->size())+" ("+ConvToStr(ServerInstance->chanlist->size()*sizeof(chanrec))+" bytes)");
			results.push_back(sn+" 249 "+user->nick+" :Commands(VECTOR) "+ConvToStr(ServerInstance->Parser->cmdlist.size())+" ("+ConvToStr(ServerInstance->Parser->cmdlist.size()*sizeof(command_t))+" bytes)");

			if (!ServerInstance->Config->WhoWasGroupSize == 0 && !ServerInstance->Config->WhoWasMaxGroups == 0)
			{
				command_t* whowas_command = ServerInstance->Parser->GetHandler("WHOWAS");
				if (whowas_command)
				{
					std::deque<classbase*> params;
					Extensible whowas_stats;
					params.push_back(&whowas_stats);
					whowas_command->HandleInternal(WHOWAS_STATS, params);
					if (whowas_stats.GetExt("stats"))
					{
						char* stats;
						whowas_stats.GetExt("stats", stats);
						results.push_back(sn+" 249 "+user->nick+" :"+ConvToStr(stats));
					}
				}
			}

			results.push_back(sn+" 249 "+user->nick+" :MOTD(VECTOR) "+ConvToStr(ServerInstance->Config->MOTD.size())+", RULES(VECTOR) "+ConvToStr(ServerInstance->Config->RULES.size()));
			results.push_back(sn+" 249 "+user->nick+" :Modules(VECTOR) "+ConvToStr(ServerInstance->modules.size())+" ("+ConvToStr(ServerInstance->modules.size()*sizeof(Module))+" bytes)");
			results.push_back(sn+" 249 "+user->nick+" :ClassFactories(VECTOR) "+ConvToStr(ServerInstance->factory.size())+" ("+ConvToStr(ServerInstance->factory.size()*sizeof(ircd_module))+" bytes)");

#ifndef WIN32
			/* Moved this down here so all the not-windows stuff (look w00tie, I didn't say win32!) is in one ifndef.
			 * Also cuts out some identical code in both branches of the ifndef. -- Om
			 */
			rusage R;

			/* Not sure why we were doing '0' with a RUSAGE_SELF comment rather than just using RUSAGE_SELF -- Om */
			if (!getrusage(RUSAGE_SELF,&R))	/* RUSAGE_SELF */
			{
				results.push_back(sn+" 249 "+user->nick+" :Total allocation: "+ConvToStr(R.ru_maxrss)+"K");
				results.push_back(sn+" 249 "+user->nick+" :Signals:          "+ConvToStr(R.ru_nsignals));
				results.push_back(sn+" 249 "+user->nick+" :Page faults:      "+ConvToStr(R.ru_majflt));
				results.push_back(sn+" 249 "+user->nick+" :Swaps:            "+ConvToStr(R.ru_nswap));
				results.push_back(sn+" 249 "+user->nick+" :Context Switches: Voluntary; "+ConvToStr(R.ru_nvcsw)+" Involuntary; "+ConvToStr(R.ru_nivcsw));

				timeval tv;
				char percent[30];
				gettimeofday(&tv, NULL);
			
				float n_elapsed = ((tv.tv_sec - ServerInstance->stats->LastSampled.tv_sec) * 1000000 + tv.tv_usec - ServerInstance->stats->LastSampled.tv_usec);
				float n_eaten = ((R.ru_utime.tv_sec - ServerInstance->stats->LastCPU.tv_sec) * 1000000 + R.ru_utime.tv_usec - ServerInstance->stats->LastCPU.tv_usec);
				float per = (n_eaten / n_elapsed) * 100;

				snprintf(percent, 30, "%03.5f%%", per);
				results.push_back(sn+" 249 "+user->nick+" :CPU Usage: "+percent);
			}
#endif
		}
		break;
	
		case 'T':
		{
			char buffer[MAXBUF];
			results.push_back(sn+" 249 "+user->nick+" :accepts "+ConvToStr(ServerInstance->stats->statsAccept)+" refused "+ConvToStr(ServerInstance->stats->statsRefused));
			results.push_back(sn+" 249 "+user->nick+" :unknown commands "+ConvToStr(ServerInstance->stats->statsUnknown));
			results.push_back(sn+" 249 "+user->nick+" :nick collisions "+ConvToStr(ServerInstance->stats->statsCollisions));
			results.push_back(sn+" 249 "+user->nick+" :dns requests "+ConvToStr(ServerInstance->stats->statsDnsGood+ServerInstance->stats->statsDnsBad)+" succeeded "+ConvToStr(ServerInstance->stats->statsDnsGood)+" failed "+ConvToStr(ServerInstance->stats->statsDnsBad));
			results.push_back(sn+" 249 "+user->nick+" :connection count "+ConvToStr(ServerInstance->stats->statsConnects));
			snprintf(buffer,MAXBUF," 249 %s :bytes sent %5.2fK recv %5.2fK",user->nick,ServerInstance->stats->statsSent / 1024,ServerInstance->stats->statsRecv / 1024);
			results.push_back(sn+buffer);
		}
		break;

		/* stats o */
		case 'o':
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
		break;

		/* stats l (show user I/O stats) */
		case 'l':
			results.push_back(sn+" 211 "+user->nick+" :nick[ident@host] sendq cmds_out bytes_out cmds_in bytes_in time_open");
		  	for (std::vector<userrec*>::iterator n = ServerInstance->local_users.begin(); n != ServerInstance->local_users.end(); n++)
			{
				userrec* i = *n;
				if (ServerInstance->IsNick(i->nick))
				{
					results.push_back(sn+" 211 "+user->nick+" "+i->nick+"["+i->ident+"@"+i->dhost+"] "+ConvToStr(i->sendq.length())+" "+ConvToStr(i->cmds_out)+" "+ConvToStr(i->bytes_out)+" "+ConvToStr(i->cmds_in)+" "+ConvToStr(i->bytes_in)+" "+ConvToStr(ServerInstance->Time() - i->age));
				}
			}
		break;

	/* stats L (show user I/O stats with IP addresses) */
		case 'L':
			results.push_back(sn+" 211 "+user->nick+" :nick[ident@ip] sendq cmds_out bytes_out cmds_in bytes_in time_open");
			for (std::vector<userrec*>::iterator n = ServerInstance->local_users.begin(); n != ServerInstance->local_users.end(); n++)
			{
				userrec* i = *n;
				if (ServerInstance->IsNick(i->nick))
				{
					results.push_back(sn+" 211 "+user->nick+" "+i->nick+"["+i->ident+"@"+i->GetIPString()+"] "+ConvToStr(i->sendq.length())+" "+ConvToStr(i->cmds_out)+" "+ConvToStr(i->bytes_out)+" "+ConvToStr(i->cmds_in)+" "+ConvToStr(i->bytes_in)+" "+ConvToStr(ServerInstance->Time() - i->age));
				}
			}
		break;

		/* stats u (show server uptime) */
		case 'u':
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
		break;

		default:
		break;
	}

	results.push_back(sn+" 219 "+user->nick+" "+statschar+" :End of /STATS report");
	ServerInstance->SNO->WriteToSnoMask('t',"%s '%c' requested by %s (%s@%s)",(!strcmp(user->server,ServerInstance->Config->ServerName) ? "Stats" : "Remote stats"),statschar,user->nick,user->ident,user->host);

	return;
}

