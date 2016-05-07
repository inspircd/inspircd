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
	void DoStats(Stats::Context& stats);
 public:
	/** Constructor for stats.
	 */
	CommandStats ( Module* parent) : Command(parent,"STATS",1,2) { allow_empty_last_param = false; syntax = "<stats-symbol> [<servername>]"; }
	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		if ((parameters.size() > 1) && (parameters[1].find('.') != std::string::npos))
			return ROUTE_UNICAST(parameters[1]);
		return ROUTE_LOCALONLY;
	}
};

static void GenerateStatsLl(Stats::Context& stats)
{
	stats.AddRow(211, InspIRCd::Format("nick[ident@%s] sendq cmds_out bytes_out cmds_in bytes_in time_open", (stats.GetSymbol() == 'l' ? "host" : "ip")));

	const UserManager::LocalList& list = ServerInstance->Users.GetLocalUsers();
	for (UserManager::LocalList::const_iterator i = list.begin(); i != list.end(); ++i)
	{
		LocalUser* u = *i;
		stats.AddRow(211, u->nick+"["+u->ident+"@"+(stats.GetSymbol() == 'l' ? u->dhost : u->GetIPString())+"] "+ConvToStr(u->eh.getSendQSize())+" "+ConvToStr(u->cmds_out)+" "+ConvToStr(u->bytes_out)+" "+ConvToStr(u->cmds_in)+" "+ConvToStr(u->bytes_in)+" "+ConvToStr(ServerInstance->Time() - u->signon));
	}
}

void CommandStats::DoStats(Stats::Context& stats)
{
	User* const user = stats.GetSource();
	const char statschar = stats.GetSymbol();

	bool isPublic = ServerInstance->Config->UserStats.find(statschar) != std::string::npos;
	bool isRemoteOper = IS_REMOTE(user) && (user->IsOper());
	bool isLocalOperWithPrivs = IS_LOCAL(user) && user->HasPrivPermission("servers/auspex");

	if (!isPublic && !isRemoteOper && !isLocalOperWithPrivs)
	{
		ServerInstance->SNO->WriteToSnoMask('t',
				"%s '%c' denied for %s (%s@%s)",
				(IS_LOCAL(user) ? "Stats" : "Remote stats"),
				statschar, user->nick.c_str(), user->ident.c_str(), user->host.c_str());
		stats.AddRow(481, (std::string("Permission Denied - STATS ") + statschar + " requires the servers/auspex priv."));
		return;
	}

	ModResult MOD_RESULT;
	FIRST_MOD_RESULT(OnStats, MOD_RESULT, (stats));
	if (MOD_RESULT == MOD_RES_DENY)
	{
		stats.AddRow(219, statschar, "End of /STATS report");
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

				stats.AddRow(249, ip + ":"+ConvToStr(ls->bind_port) + " (" + type + ", " + hook + ")");
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
				Stats::Row row(215);
				row.push("I").push(c->name);

				std::string param;
				if (c->type == CC_ALLOW)
					param.push_back('+');
				if (c->type == CC_DENY)
					param.push_back('-');

				if (c->type == CC_NAMED)
					param.push_back('*');
				else
					param.append(c->host);

				row.push(param).push(c->config->getString("port", "*"));
				row.push(ConvToStr(c->GetRecvqMax())).push(ConvToStr(c->GetSendqSoftMax())).push(ConvToStr(c->GetSendqHardMax())).push(ConvToStr(c->GetCommandRate()));

				param = ConvToStr(c->GetPenaltyThreshold());
				if (c->fakelag)
					param.push_back('*');
				row.push(param);

				stats.AddRow(row);
			}
		}
		break;

		case 'Y':
		{
			int idx = 0;
			for (ServerConfig::ClassVector::const_iterator i = ServerInstance->Config->Classes.begin(); i != ServerInstance->Config->Classes.end(); i++)
			{
				ConnectClass* c = *i;
				stats.AddRow(215, 'i', "NOMATCH", '*', c->GetHost(), (c->limit ? c->limit : SocketEngine::GetMaxFds()), idx, ServerInstance->Config->ServerName, '*');
				stats.AddRow(218, 'Y', idx, c->GetPingTime(), '0', c->GetSendqHardMax(), ConvToStr(c->GetRecvqMax())+" "+ConvToStr(c->GetRegTimeout()));
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
					stats.AddRow(249, oper->nick + " (" + oper->ident + "@" + oper->dhost + ") Idle: " +
							(lu ? ConvToStr(ServerInstance->Time() - lu->idle_lastmsg) + " secs" : "unavailable"));
					idx++;
				}
			}
			stats.AddRow(249, ConvToStr(idx)+" OPER(s)");
		}
		break;

		case 'k':
			ServerInstance->XLines->InvokeStats("K",216,stats);
		break;
		case 'g':
			ServerInstance->XLines->InvokeStats("G",223,stats);
		break;
		case 'q':
			ServerInstance->XLines->InvokeStats("Q",217,stats);
		break;
		case 'Z':
			ServerInstance->XLines->InvokeStats("Z",223,stats);
		break;
		case 'e':
			ServerInstance->XLines->InvokeStats("E",223,stats);
		break;
		case 'E':
		{
			const SocketEngine::Statistics& sestats = SocketEngine::GetStats();
			stats.AddRow(249, "Total events: "+ConvToStr(sestats.TotalEvents));
			stats.AddRow(249, "Read events:  "+ConvToStr(sestats.ReadEvents));
			stats.AddRow(249, "Write events: "+ConvToStr(sestats.WriteEvents));
			stats.AddRow(249, "Error events: "+ConvToStr(sestats.ErrorEvents));
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
					stats.AddRow(212, i->second->name, i->second->use_count);
				}
			}
		}
		break;

		/* stats z (debug and memory info) */
		case 'z':
		{
			stats.AddRow(249, "Users: "+ConvToStr(ServerInstance->Users->GetUsers().size()));
			stats.AddRow(249, "Channels: "+ConvToStr(ServerInstance->GetChans().size()));
			stats.AddRow(249, "Commands: "+ConvToStr(ServerInstance->Parser.GetCommands().size()));

			float kbitpersec_in, kbitpersec_out, kbitpersec_total;
			SocketEngine::GetStats().GetBandwidth(kbitpersec_in, kbitpersec_out, kbitpersec_total);

			stats.AddRow(249, InspIRCd::Format("Bandwidth total:  %03.5f kilobits/sec", kbitpersec_total));
			stats.AddRow(249, InspIRCd::Format("Bandwidth out:    %03.5f kilobits/sec", kbitpersec_out));
			stats.AddRow(249, InspIRCd::Format("Bandwidth in:     %03.5f kilobits/sec", kbitpersec_in));

#ifndef _WIN32
			/* Moved this down here so all the not-windows stuff (look w00tie, I didn't say win32!) is in one ifndef.
			 * Also cuts out some identical code in both branches of the ifndef. -- Om
			 */
			rusage R;

			/* Not sure why we were doing '0' with a RUSAGE_SELF comment rather than just using RUSAGE_SELF -- Om */
			if (!getrusage(RUSAGE_SELF,&R))	/* RUSAGE_SELF */
			{
				stats.AddRow(249, "Total allocation: "+ConvToStr(R.ru_maxrss)+"K");
				stats.AddRow(249, "Signals:          "+ConvToStr(R.ru_nsignals));
				stats.AddRow(249, "Page faults:      "+ConvToStr(R.ru_majflt));
				stats.AddRow(249, "Swaps:            "+ConvToStr(R.ru_nswap));
				stats.AddRow(249, "Context Switches: Voluntary; "+ConvToStr(R.ru_nvcsw)+" Involuntary; "+ConvToStr(R.ru_nivcsw));

				float n_elapsed = (ServerInstance->Time() - ServerInstance->stats.LastSampled.tv_sec) * 1000000
					+ (ServerInstance->Time_ns() - ServerInstance->stats.LastSampled.tv_nsec) / 1000;
				float n_eaten = ((R.ru_utime.tv_sec - ServerInstance->stats.LastCPU.tv_sec) * 1000000 + R.ru_utime.tv_usec - ServerInstance->stats.LastCPU.tv_usec);
				float per = (n_eaten / n_elapsed) * 100;

				stats.AddRow(249, InspIRCd::Format("CPU Use (now):    %03.5f%%", per));

				n_elapsed = ServerInstance->Time() - ServerInstance->startup_time;
				n_eaten = (float)R.ru_utime.tv_sec + R.ru_utime.tv_usec / 100000.0;
				per = (n_eaten / n_elapsed) * 100;

				stats.AddRow(249, InspIRCd::Format("CPU Use (total):  %03.5f%%", per));
			}
#else
			PROCESS_MEMORY_COUNTERS MemCounters;
			if (GetProcessMemoryInfo(GetCurrentProcess(), &MemCounters, sizeof(MemCounters)))
			{
				stats.AddRow(249, "Total allocation: "+ConvToStr((MemCounters.WorkingSetSize + MemCounters.PagefileUsage) / 1024)+"K");
				stats.AddRow(249, "Pagefile usage:   "+ConvToStr(MemCounters.PagefileUsage / 1024)+"K");
				stats.AddRow(249, "Page faults:      "+ConvToStr(MemCounters.PageFaultCount));
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

				stats.AddRow(249, InspIRCd::Format("CPU Use (now):    %03.5f%%", per));

				n_elapsed = ServerInstance->Time() - ServerInstance->startup_time;
				n_eaten = (double)(( (uint64_t)(KernelTime.dwHighDateTime) << 32 ) + (uint64_t)(KernelTime.dwLowDateTime))/100000;
				per = (n_eaten / n_elapsed);

				stats.AddRow(249, InspIRCd::Format("CPU Use (total):  %03.5f%%", per));
			}
#endif
		}
		break;

		case 'T':
		{
			stats.AddRow(249, "accepts "+ConvToStr(ServerInstance->stats.Accept)+" refused "+ConvToStr(ServerInstance->stats.Refused));
			stats.AddRow(249, "unknown commands "+ConvToStr(ServerInstance->stats.Unknown));
			stats.AddRow(249, "nick collisions "+ConvToStr(ServerInstance->stats.Collisions));
			stats.AddRow(249, "dns requests "+ConvToStr(ServerInstance->stats.DnsGood+ServerInstance->stats.DnsBad)+" succeeded "+ConvToStr(ServerInstance->stats.DnsGood)+" failed "+ConvToStr(ServerInstance->stats.DnsBad));
			stats.AddRow(249, "connection count "+ConvToStr(ServerInstance->stats.Connects));
			stats.AddRow(249, InspIRCd::Format("bytes sent %5.2fK recv %5.2fK",
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
				stats.AddRow(243, 'O', tag->getString("host"), '*', tag->getString("name"), tag->getString("type"), '0');
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
				stats.AddRow(243, 'O', tag->name, umodes, cmodes);
			}
		}
		break;

		/* stats l (show user I/O stats) */
		case 'l':
		/* stats L (show user I/O stats with IP addresses) */
		case 'L':
			GenerateStatsLl(stats);
		break;

		/* stats u (show server uptime) */
		case 'u':
		{
			unsigned int up = static_cast<unsigned int>(ServerInstance->Time() - ServerInstance->startup_time);
			stats.AddRow(242, InspIRCd::Format("Server up %u days, %.2u:%.2u:%.2u",
				up / 86400, (up / 3600) % 24, (up / 60) % 60, up % 60));
		}
		break;

		default:
		break;
	}

	stats.AddRow(219, statschar, "End of /STATS report");
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
	Stats::Context stats(user, parameters[0][0]);
	DoStats(stats);
	const std::vector<Stats::Row>& rows = stats.GetRows();
	for (std::vector<Stats::Row>::const_iterator i = rows.begin(); i != rows.end(); ++i)
	{
		const Stats::Row& row = *i;
		user->WriteRemoteNumeric(row);
	}

	return CMD_SUCCESS;
}

COMMAND_INIT(CommandStats)
