/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"
#include "xline.h"

#ifdef _WIN32
#include <psapi.h>
#pragma comment(lib, "psapi.lib") // For GetProcessMemoryInfo()
#endif

/** Handle /STATS.
 */
class CommandStats : public Command
{
	void DoStats(char statschar, User* user, string_list &results);
 public:
	/** Constructor for stats.
	 */
	CommandStats ( Module* parent) : Command(parent,"STATS",1,2) { syntax = "<stats-symbol> [<servername>]"; }
	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		if (parameters.size() > 1)
			return ROUTE_UNICAST(parameters[1]);
		return ROUTE_LOCALONLY;
	}
};

static void GenerateStatsLl(User* user, string_list& results, char c)
{
	results.push_back(InspIRCd::Format("211 %s nick[ident@%s] sendq cmds_out bytes_out cmds_in bytes_in time_open", user->nick.c_str(), (c == 'l' ? "host" : "ip")));

	const UserManager::LocalList& list = ServerInstance->Users.GetLocalUsers();
	for (UserManager::LocalList::const_iterator i = list.begin(); i != list.end(); ++i)
	{
		LocalUser* u = *i;
		results.push_back("211 "+user->nick+" "+u->nick+"["+u->ident+"@"+(c == 'l' ? u->dhost : u->GetIPString())+"] "+ConvToStr(u->eh.getSendQSize())+" "+ConvToStr(u->cmds_out)+" "+ConvToStr(u->bytes_out)+" "+ConvToStr(u->cmds_in)+" "+ConvToStr(u->bytes_in)+" "+ConvToStr(ServerInstance->Time() - u->signon));
	}
}

void CommandStats::DoStats(char statschar, User* user, string_list &results)
{
	bool isPublic = ServerInstance->Config->UserStats.find(statschar) != std::string::npos;
	bool isRemoteOper = IS_REMOTE(user) && (user->IsOper());
	bool isLocalOperWithPrivs = IS_LOCAL(user) && user->HasPrivPermission("servers/auspex");

	if (!isPublic && !isRemoteOper && !isLocalOperWithPrivs)
	{
		ServerInstance->SNO->WriteToSnoMask('t',
				"%s '%c' denied for %s (%s@%s)",
				(IS_LOCAL(user) ? "Stats" : "Remote stats"),
				statschar, user->nick.c_str(), user->ident.c_str(), user->host.c_str());
		results.push_back("481 " + user->nick + " :Permission Denied - STATS " + statschar + " requires the servers/auspex priv.");
		return;
	}

	ModResult MOD_RESULT;
	FIRST_MOD_RESULT(OnStats, MOD_RESULT, (statschar, user, results));
	if (MOD_RESULT == MOD_RES_DENY)
	{
		results.push_back("219 "+user->nick+" "+statschar+" :End of /STATS report");
		ServerInstance->SNO->WriteToSnoMask('t',"%s '%c' requested by %s (%s@%s)",
			(IS_LOCAL(user) ? "Stats" : "Remote stats"), statschar, user->nick.c_str(), user->ident.c_str(), user->host.c_str());
		return;
	}

	switch (statschar)
	{
		/* stats p (show listening ports) */
		case 'p':
		{
			for (std::vector<ListenSocket*>::const_iterator i = ServerInstance->ports.begin(); i != ServerInstance->ports.end(); ++i)
			{
				ListenSocket* ls = *i;
				std::string ip = ls->bind_addr;
				if (ip.empty())
					ip.assign("*");
				else if (ip.find_first_of(':') != std::string::npos)
				{
					ip.insert(ip.begin(), '[');
					ip.insert(ip.end(),  ']');
				}
				std::string type = ls->bind_tag->getString("type", "clients");
				std::string hook = ls->bind_tag->getString("ssl", "plaintext");

				results.push_back("249 "+user->nick+" :"+ ip + ":"+ConvToStr(ls->bind_port)+
					" (" + type + ", " + hook + ")");
			}
		}
		break;

		/* These stats symbols must be handled by a linking module */
		case 'n':
		case 'c':
		break;

		case 'i':
		{
			for (ServerConfig::ClassVector::const_iterator i = ServerInstance->Config->Classes.begin(); i != ServerInstance->Config->Classes.end(); ++i)
			{
				ConnectClass* c = *i;
				std::stringstream res;
				res << "215 " << user->nick << " I " << c->name << ' ';
				if (c->type == CC_ALLOW)
					res << '+';
				if (c->type == CC_DENY)
					res << '-';

				if (c->type == CC_NAMED)
					res << '*';
				else
					res << c->host;

				res << ' ' << c->config->getString("port", "*") << ' ';

				res << c->GetRecvqMax() << ' ' << c->GetSendqSoftMax() << ' ' << c->GetSendqHardMax()
					<< ' ' << c->GetCommandRate() << ' ' << c->GetPenaltyThreshold();
				if (c->fakelag)
					res << '*';
				results.push_back(res.str());
			}
		}
		break;

		case 'Y':
		{
			int idx = 0;
			for (ServerConfig::ClassVector::const_iterator i = ServerInstance->Config->Classes.begin(); i != ServerInstance->Config->Classes.end(); i++)
			{
				ConnectClass* c = *i;
				results.push_back("215 "+user->nick+" i NOMATCH * "+c->GetHost()+" "+ConvToStr(c->limit ? c->limit : SocketEngine::GetMaxFds())+" "+ConvToStr(idx)+" "+ServerInstance->Config->ServerName+" *");
				results.push_back("218 "+user->nick+" Y "+ConvToStr(idx)+" "+ConvToStr(c->GetPingTime())+" 0 "+ConvToStr(c->GetSendqHardMax())+" :"+
						ConvToStr(c->GetRecvqMax())+" "+ConvToStr(c->GetRegTimeout()));
				idx++;
			}
		}
		break;

		case 'P':
		{
			unsigned int idx = 0;
			const UserManager::OperList& opers = ServerInstance->Users->all_opers;
			for (UserManager::OperList::const_iterator i = opers.begin(); i != opers.end(); ++i)
			{
				User* oper = *i;
				if (!oper->server->IsULine())
				{
					LocalUser* lu = IS_LOCAL(oper);
					results.push_back("249 " + user->nick + " :" + oper->nick + " (" + oper->ident + "@" + oper->dhost + ") Idle: " +
							(lu ? ConvToStr(ServerInstance->Time() - lu->idle_lastmsg) + " secs" : "unavailable"));
					idx++;
				}
			}
			results.push_back("249 "+user->nick+" :"+ConvToStr(idx)+" OPER(s)");
		}
		break;

		case 'k':
			ServerInstance->XLines->InvokeStats("K",216,user,results);
		break;
		case 'g':
			ServerInstance->XLines->InvokeStats("G",223,user,results);
		break;
		case 'q':
			ServerInstance->XLines->InvokeStats("Q",217,user,results);
		break;
		case 'Z':
			ServerInstance->XLines->InvokeStats("Z",223,user,results);
		break;
		case 'e':
			ServerInstance->XLines->InvokeStats("E",223,user,results);
		break;
		case 'E':
		{
			const SocketEngine::Statistics& stats = SocketEngine::GetStats();
			results.push_back("249 "+user->nick+" :Total events: "+ConvToStr(stats.TotalEvents));
			results.push_back("249 "+user->nick+" :Read events:  "+ConvToStr(stats.ReadEvents));
			results.push_back("249 "+user->nick+" :Write events: "+ConvToStr(stats.WriteEvents));
			results.push_back("249 "+user->nick+" :Error events: "+ConvToStr(stats.ErrorEvents));
			break;
		}

		/* stats m (list number of times each command has been used, plus bytecount) */
		case 'm':
		{
			const CommandParser::CommandMap& commands = ServerInstance->Parser.GetCommands();
			for (CommandParser::CommandMap::const_iterator i = commands.begin(); i != commands.end(); ++i)
			{
				if (i->second->use_count)
				{
					/* RPL_STATSCOMMANDS */
					results.push_back("212 "+user->nick+" "+i->second->name+" "+ConvToStr(i->second->use_count));
				}
			}
		}
		break;

		/* stats z (debug and memory info) */
		case 'z':
		{
			results.push_back("249 "+user->nick+" :Users: "+ConvToStr(ServerInstance->Users->GetUsers().size()));
			results.push_back("249 "+user->nick+" :Channels: "+ConvToStr(ServerInstance->GetChans().size()));
			results.push_back("249 "+user->nick+" :Commands: "+ConvToStr(ServerInstance->Parser.GetCommands().size()));

			float kbitpersec_in, kbitpersec_out, kbitpersec_total;
			char kbitpersec_in_s[30], kbitpersec_out_s[30], kbitpersec_total_s[30];

			SocketEngine::GetStats().GetBandwidth(kbitpersec_in, kbitpersec_out, kbitpersec_total);

			snprintf(kbitpersec_total_s, 30, "%03.5f", kbitpersec_total);
			snprintf(kbitpersec_out_s, 30, "%03.5f", kbitpersec_out);
			snprintf(kbitpersec_in_s, 30, "%03.5f", kbitpersec_in);

			results.push_back("249 "+user->nick+" :Bandwidth total:  "+ConvToStr(kbitpersec_total_s)+" kilobits/sec");
			results.push_back("249 "+user->nick+" :Bandwidth out:    "+ConvToStr(kbitpersec_out_s)+" kilobits/sec");
			results.push_back("249 "+user->nick+" :Bandwidth in:     "+ConvToStr(kbitpersec_in_s)+" kilobits/sec");

#ifndef _WIN32
			/* Moved this down here so all the not-windows stuff (look w00tie, I didn't say win32!) is in one ifndef.
			 * Also cuts out some identical code in both branches of the ifndef. -- Om
			 */
			rusage R;

			/* Not sure why we were doing '0' with a RUSAGE_SELF comment rather than just using RUSAGE_SELF -- Om */
			if (!getrusage(RUSAGE_SELF,&R))	/* RUSAGE_SELF */
			{
				results.push_back("249 "+user->nick+" :Total allocation: "+ConvToStr(R.ru_maxrss)+"K");
				results.push_back("249 "+user->nick+" :Signals:          "+ConvToStr(R.ru_nsignals));
				results.push_back("249 "+user->nick+" :Page faults:      "+ConvToStr(R.ru_majflt));
				results.push_back("249 "+user->nick+" :Swaps:            "+ConvToStr(R.ru_nswap));
				results.push_back("249 "+user->nick+" :Context Switches: Voluntary; "+ConvToStr(R.ru_nvcsw)+" Involuntary; "+ConvToStr(R.ru_nivcsw));

				char percent[30];

				float n_elapsed = (ServerInstance->Time() - ServerInstance->stats.LastSampled.tv_sec) * 1000000
					+ (ServerInstance->Time_ns() - ServerInstance->stats.LastSampled.tv_nsec) / 1000;
				float n_eaten = ((R.ru_utime.tv_sec - ServerInstance->stats.LastCPU.tv_sec) * 1000000 + R.ru_utime.tv_usec - ServerInstance->stats.LastCPU.tv_usec);
				float per = (n_eaten / n_elapsed) * 100;

				snprintf(percent, 30, "%03.5f%%", per);
				results.push_back("249 "+user->nick+" :CPU Use (now):    "+percent);

				n_elapsed = ServerInstance->Time() - ServerInstance->startup_time;
				n_eaten = (float)R.ru_utime.tv_sec + R.ru_utime.tv_usec / 100000.0;
				per = (n_eaten / n_elapsed) * 100;
				snprintf(percent, 30, "%03.5f%%", per);
				results.push_back("249 "+user->nick+" :CPU Use (total):  "+percent);
			}
#else
			PROCESS_MEMORY_COUNTERS MemCounters;
			if (GetProcessMemoryInfo(GetCurrentProcess(), &MemCounters, sizeof(MemCounters)))
			{
				results.push_back("249 "+user->nick+" :Total allocation: "+ConvToStr((MemCounters.WorkingSetSize + MemCounters.PagefileUsage) / 1024)+"K");
				results.push_back("249 "+user->nick+" :Pagefile usage:   "+ConvToStr(MemCounters.PagefileUsage / 1024)+"K");
				results.push_back("249 "+user->nick+" :Page faults:      "+ConvToStr(MemCounters.PageFaultCount));
			}

			FILETIME CreationTime;
			FILETIME ExitTime;
			FILETIME KernelTime;
			FILETIME UserTime;
			LARGE_INTEGER ThisSample;
			if(GetProcessTimes(GetCurrentProcess(), &CreationTime, &ExitTime, &KernelTime, &UserTime) &&
				QueryPerformanceCounter(&ThisSample))
			{
				KernelTime.dwHighDateTime += UserTime.dwHighDateTime;
				KernelTime.dwLowDateTime += UserTime.dwLowDateTime;
				double n_eaten = (double)( ( (uint64_t)(KernelTime.dwHighDateTime - ServerInstance->stats.LastCPU.dwHighDateTime) << 32 ) + (uint64_t)(KernelTime.dwLowDateTime - ServerInstance->stats.LastCPU.dwLowDateTime) )/100000;
				double n_elapsed = (double)(ThisSample.QuadPart - ServerInstance->stats.LastSampled.QuadPart) / ServerInstance->stats.QPFrequency.QuadPart;
				double per = (n_eaten/n_elapsed);

				char percent[30];

				snprintf(percent, 30, "%03.5f%%", per);
				results.push_back("249 "+user->nick+" :CPU Use (now):    "+percent);

				n_elapsed = ServerInstance->Time() - ServerInstance->startup_time;
				n_eaten = (double)(( (uint64_t)(KernelTime.dwHighDateTime) << 32 ) + (uint64_t)(KernelTime.dwLowDateTime))/100000;
				per = (n_eaten / n_elapsed);
				snprintf(percent, 30, "%03.5f%%", per);
				results.push_back("249 "+user->nick+" :CPU Use (total):  "+percent);
			}
#endif
		}
		break;

		case 'T':
		{
			results.push_back("249 "+user->nick+" :accepts "+ConvToStr(ServerInstance->stats.Accept)+" refused "+ConvToStr(ServerInstance->stats.Refused));
			results.push_back("249 "+user->nick+" :unknown commands "+ConvToStr(ServerInstance->stats.Unknown));
			results.push_back("249 "+user->nick+" :nick collisions "+ConvToStr(ServerInstance->stats.Collisions));
			results.push_back("249 "+user->nick+" :dns requests "+ConvToStr(ServerInstance->stats.DnsGood+ServerInstance->stats.DnsBad)+" succeeded "+ConvToStr(ServerInstance->stats.DnsGood)+" failed "+ConvToStr(ServerInstance->stats.DnsBad));
			results.push_back("249 "+user->nick+" :connection count "+ConvToStr(ServerInstance->stats.Connects));
			results.push_back(InspIRCd::Format("249 %s :bytes sent %5.2fK recv %5.2fK", user->nick.c_str(),
				ServerInstance->stats.Sent / 1024.0, ServerInstance->stats.Recv / 1024.0));
		}
		break;

		/* stats o */
		case 'o':
		{
			ConfigTagList tags = ServerInstance->Config->ConfTags("oper");
			for(ConfigIter i = tags.first; i != tags.second; ++i)
			{
				ConfigTag* tag = i->second;
				results.push_back("243 "+user->nick+" O "+tag->getString("host")+" * "+
					tag->getString("name") + " " + tag->getString("type")+" 0");
			}
		}
		break;
		case 'O':
		{
			for (ServerConfig::OperIndex::const_iterator i = ServerInstance->Config->OperTypes.begin(); i != ServerInstance->Config->OperTypes.end(); ++i)
			{
				OperInfo* tag = i->second;
				tag->init();
				std::string umodes;
				std::string cmodes;
				for(char c='A'; c <= 'z'; c++)
				{
					ModeHandler* mh = ServerInstance->Modes->FindMode(c, MODETYPE_USER);
					if (mh && mh->NeedsOper() && tag->AllowedUserModes[c - 'A'])
						umodes.push_back(c);
					mh = ServerInstance->Modes->FindMode(c, MODETYPE_CHANNEL);
					if (mh && mh->NeedsOper() && tag->AllowedChanModes[c - 'A'])
						cmodes.push_back(c);
				}
				results.push_back("243 "+user->nick+" O "+tag->name.c_str() + " " + umodes + " " + cmodes);
			}
		}
		break;

		/* stats l (show user I/O stats) */
		case 'l':
		/* stats L (show user I/O stats with IP addresses) */
		case 'L':
			GenerateStatsLl(user, results, statschar);
		break;

		/* stats u (show server uptime) */
		case 'u':
		{
			unsigned int up = static_cast<unsigned int>(ServerInstance->Time() - ServerInstance->startup_time);
			results.push_back(InspIRCd::Format("242 %s :Server up %u days, %.2u:%.2u:%.2u", user->nick.c_str(),
				up / 86400, (up / 3600) % 24, (up / 60) % 60, up % 60));
		}
		break;

		default:
		break;
	}

	results.push_back("219 "+user->nick+" "+statschar+" :End of /STATS report");
	ServerInstance->SNO->WriteToSnoMask('t',"%s '%c' requested by %s (%s@%s)",
		(IS_LOCAL(user) ? "Stats" : "Remote stats"), statschar, user->nick.c_str(), user->ident.c_str(), user->host.c_str());
	return;
}

CmdResult CommandStats::Handle (const std::vector<std::string>& parameters, User *user)
{
	if (parameters.size() > 1 && parameters[1] != ServerInstance->Config->ServerName)
	{
		// Give extra penalty if a non-oper does /STATS <remoteserver>
		LocalUser* localuser = IS_LOCAL(user);
		if ((localuser) && (!user->IsOper()))
			localuser->CommandFloodPenalty += 2000;
		return CMD_SUCCESS;
	}
	string_list values;
	char search = parameters[0][0];
	DoStats(search, user, values);

	const std::string p = ":" + ServerInstance->Config->ServerName + " ";
	for (size_t i = 0; i < values.size(); i++)
		user->SendText(p + values[i]);

	return CMD_SUCCESS;
}

COMMAND_INIT(CommandStats)
