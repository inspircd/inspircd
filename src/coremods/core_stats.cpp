	/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 Puck Meerburg <puck@puckipedia.com>
 *   Copyright (C) 2016, 2018-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2012 Adam <Adam@anope.org>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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
#include "modules/cap.h"
#include "modules/stats.h"
#include "utility/string.h"
#include "xline.h"

#ifdef _WIN32
# include <psapi.h>
#else
# include <netinet/in.h>
# include <sys/resource.h>
#endif

class StatsTagsProvider
	: public ClientProtocol::MessageTagProvider
{
private:
	Cap::Capability statscap;

public:
	StatsTagsProvider(Module* mod)
		: ClientProtocol::MessageTagProvider(mod)
		, statscap(mod, "inspircd.org/stats-tags")
	{
	}

	bool ShouldSendTag(LocalUser* user, const ClientProtocol::MessageTagData& tagdata) override
	{
		return statscap.IsEnabled(user);
	}
};


class CommandStats final
	: public Command
{
private:
	Events::ModuleEventProvider statsevprov;
	StatsTagsProvider statstags;

	void DoStats(Stats::Context& stats);

public:
	/** STATS characters which non-opers can request. */
	std::string userstats;

	CommandStats(Module* Creator)
		: Command(Creator, "STATS", 1, 2)
		, statsevprov(Creator, "event/stats")
		, statstags(Creator)
	{
		syntax = { "<symbol> [<servername>]" };
	}

	CmdResult Handle(User* user, const Params& parameters) override;

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		if ((parameters.size() > 1) && (parameters[1].find('.') != std::string::npos))
			return ROUTE_UNICAST(parameters[1]);
		return ROUTE_LOCALONLY;
	}
};

static void GenerateStatsLl(Stats::Context& stats)
{
	stats.AddRow(211, INSP_FORMAT("nick[user@{}] sendq cmds_out bytes_out cmds_in bytes_in time_open", stats.GetSymbol() == 'l' ? "host" : "ip"));

	for (auto* u : ServerInstance->Users.GetLocalUsers())
		stats.AddRow(211, u->nick+"["+u->GetDisplayedUser()+"@"+(stats.GetSymbol() == 'l' ? u->GetDisplayedHost() : u->GetAddress())+"] "+ConvToStr(u->eh.GetSendQSize())+" "+ConvToStr(u->cmds_out)+" "+ConvToStr(u->bytes_out)+" "+ConvToStr(u->cmds_in)+" "+ConvToStr(u->bytes_in)+" "+ConvToStr(ServerInstance->Time() - u->signon));
}

void CommandStats::DoStats(Stats::Context& stats)
{
	User* const user = stats.GetSource();
	const char statschar = stats.GetSymbol();

	bool isPublic = userstats.find(statschar) != std::string::npos;
	bool isRemoteOper = IS_REMOTE(user) && (user->IsOper());
	bool isLocalOperWithPrivs = IS_LOCAL(user) && user->HasPrivPermission("servers/auspex");

	if (!isPublic && !isRemoteOper && !isLocalOperWithPrivs)
	{
		const char* what = IS_LOCAL(user) ? "Stats" : "Remote stats";
		ServerInstance->SNO.WriteToSnoMask('t', "{} '{}' denied for {} ({})", what, statschar, user->nick, user->GetRealUserHost());
		stats.AddRow(481, (std::string("Permission Denied - STATS ") + statschar + " requires the servers/auspex priv."));
		return;
	}

	ModResult res = statsevprov.FirstResult(&Stats::EventListener::OnStats, stats);
	if (res == MOD_RES_DENY)
	{
		const char* what = IS_LOCAL(user) ? "Stats" : "Remote stats";
		ServerInstance->SNO.WriteToSnoMask('t', "{} '{}' requested by {} ({})", what, statschar, user->nick, user->GetRealUserHost());
		stats.AddRow(219, statschar, "End of /STATS report");
		return;
	}

	switch (statschar)
	{
		/* stats p (show listening ports) */
		case 'p':
		{
			for (const auto* ls : ServerInstance->ports)
			{
				std::stringstream portentry;

				const std::string type = ls->bind_tag->getString("type", "clients", 1);
				portentry << ls->bind_sa.str() << " (type: " << type;

				const char* protocol = nullptr;
				if (ls->bind_sa.family() == AF_UNIX)
					protocol = "unix";
#ifdef IPPROTO_SCTP
				else if (ls->bind_protocol == IPPROTO_SCTP)
					protocol = "sctp";
#endif
				else if (ls->bind_sa.family() == AF_INET || ls->bind_sa.family() == AF_INET6)
					protocol = "tcp";

				if (protocol)
					portentry << ", protocol: " << protocol;

				const std::string hook = ls->bind_tag->getString("hook");
				if (!hook.empty())
					portentry << ", hook: " << hook;

				const std::string sslprofile = ls->bind_tag->getString("sslprofile");
				if (!sslprofile.empty())
					portentry << ", tls profile: " << sslprofile;

				portentry << ')';
				stats.AddRow(249, portentry.str());
			}
		}
		break;

		case 'i':
		{
			for (const auto& c : ServerInstance->Config->Classes)
			{
				Stats::Row row(215);
				row.push("I").push(c->name);

				std::string param;
				if (c->type == ConnectClass::ALLOW)
					param.push_back('+');
				if (c->type == ConnectClass::DENY)
					param.push_back('-');

				if (c->type == ConnectClass::NAMED)
					param.push_back('*');
				else
					param.append(insp::join(c->GetHosts(), ','));

				row.push(param).push(c->config->getString("port", "*", 1));
				row.push(c->recvqmax).push(c->softsendqmax).push(c->hardsendqmax).push(c->commandrate);

				param = ConvToStr(c->penaltythreshold);
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
			for (const auto& c : ServerInstance->Config->Classes)
			{
				for (const auto& host : c->GetHosts())
					stats.AddRow(215, 'i', "NOMATCH", '*', host, (c->limit ? c->limit : SocketEngine::GetMaxFds()), idx, ServerInstance->Config->ServerName, '*');
				stats.AddRow(218, 'Y', idx, c->pingtime, '0', c->hardsendqmax, ConvToStr(c->recvqmax) + " " + ConvToStr(c->connection_timeout));
				idx++;
			}
		}
		break;

		case 'k':
			ServerInstance->XLines->InvokeStats("K", stats);
		break;
		case 'g':
			ServerInstance->XLines->InvokeStats("G", stats);
		break;
		case 'q':
			ServerInstance->XLines->InvokeStats("Q", stats);
		break;
		case 'Z':
			ServerInstance->XLines->InvokeStats("Z", stats);
		break;
		case 'e':
			ServerInstance->XLines->InvokeStats("E", stats);
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
			for (const auto& [_, command] : ServerInstance->Parser.GetCommands())
			{
				if (command->use_count)
				{
					/* RPL_STATSCOMMANDS */
					stats.AddRow(212, command->name, command->use_count);
				}
			}
		}
		break;

		/* stats z (debug and memory info) */
		case 'z':
		{
			stats.AddRow(249, "Users: "+ConvToStr(ServerInstance->Users.GetUsers().size()));
			stats.AddRow(249, "Channels: "+ConvToStr(ServerInstance->Channels.GetChans().size()));
			stats.AddRow(249, "Commands: "+ConvToStr(ServerInstance->Parser.GetCommands().size()));

			float kbitpersec_in;
			float kbitpersec_out;
			float kbitpersec_total;
			SocketEngine::GetStats().GetBandwidth(kbitpersec_in, kbitpersec_out, kbitpersec_total);

			stats.AddRow(249, INSP_FORMAT("Bandwidth total:  {:03.5} kilobits/sec", kbitpersec_total));
			stats.AddRow(249, INSP_FORMAT("Bandwidth out:    {:03.5} kilobits/sec", kbitpersec_out));
			stats.AddRow(249, INSP_FORMAT("Bandwidth in:     {:03.5} kilobits/sec", kbitpersec_in));

#ifndef _WIN32
			/* Moved this down here so all the not-windows stuff (look w00tie, I didn't say win32!) is in one ifndef.
			 * Also cuts out some identical code in both branches of the ifndef. -- Om
			 */
			rusage R;

			/* Not sure why we were doing '0' with a RUSAGE_SELF comment rather than just using RUSAGE_SELF -- Om */
			if (!getrusage(RUSAGE_SELF,&R))	/* RUSAGE_SELF */
			{
#ifndef __HAIKU__
				stats.AddRow(249, "Total allocation: "+ConvToStr(R.ru_maxrss)+"K");
				stats.AddRow(249, "Signals:          "+ConvToStr(R.ru_nsignals));
				stats.AddRow(249, "Page faults:      "+ConvToStr(R.ru_majflt));
				stats.AddRow(249, "Swaps:            "+ConvToStr(R.ru_nswap));
				stats.AddRow(249, "Context Switches: Voluntary; "+ConvToStr(R.ru_nvcsw)+" Involuntary; "+ConvToStr(R.ru_nivcsw));
#endif
				float n_elapsed = (ServerInstance->Time() - ServerInstance->Stats.LastSampled.tv_sec) * 1000000
					+ (ServerInstance->Time_ns() - ServerInstance->Stats.LastSampled.tv_nsec) / 1000;
				float n_eaten = ((R.ru_utime.tv_sec - ServerInstance->Stats.LastCPU.tv_sec) * 1000000 + R.ru_utime.tv_usec - ServerInstance->Stats.LastCPU.tv_usec);
				float per = (n_eaten / n_elapsed) * 100;

				stats.AddRow(249, INSP_FORMAT("CPU Use (now):    {:03.5}%", per));

				n_elapsed = ServerInstance->Time() - ServerInstance->startup_time;
				n_eaten = (float)R.ru_utime.tv_sec + R.ru_utime.tv_usec / 100000.0;
				per = (n_eaten / n_elapsed) * 100;

				stats.AddRow(249, INSP_FORMAT("CPU Use (total):  {:03.5}%", per));
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
				double n_eaten = (double)( ( (uint64_t)(KernelTime.dwHighDateTime - ServerInstance->Stats.LastSampled.dwHighDateTime) << 32 ) + (uint64_t)(KernelTime.dwLowDateTime - ServerInstance->Stats.LastSampled.dwLowDateTime) )/100000;
				double n_elapsed = (double)(ThisSample.QuadPart - ServerInstance->Stats.LastCPU.QuadPart) / ServerInstance->Stats.BootCPU.QuadPart;
				double per = (n_eaten/n_elapsed);

				stats.AddRow(249, INSP_FORMAT("CPU Use (now):    {:03.5}%", per));

				n_elapsed = ServerInstance->Time() - ServerInstance->startup_time;
				n_eaten = (double)(( (uint64_t)(KernelTime.dwHighDateTime) << 32 ) + (uint64_t)(KernelTime.dwLowDateTime))/100000;
				per = (n_eaten / n_elapsed);

				stats.AddRow(249, INSP_FORMAT("CPU Use (total):  {:03.5}%", per));
			}
#endif
		}
		break;

		case 'T':
		{
			stats.AddRow(249, "accepts "+ConvToStr(ServerInstance->Stats.Accept)+" refused "+ConvToStr(ServerInstance->Stats.Refused));
			stats.AddRow(249, "unknown commands "+ConvToStr(ServerInstance->Stats.Unknown));
			stats.AddRow(249, "nick collisions "+ConvToStr(ServerInstance->Stats.Collisions));
			stats.AddRow(249, "connection count "+ConvToStr(ServerInstance->Stats.Connects));
			stats.AddRow(249, INSP_FORMAT("bytes sent {:5.2}K recv {:5.2}K",
				ServerInstance->Stats.Sent / 1024.0, ServerInstance->Stats.Recv / 1024.0));
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
			stats.AddRow(242, INSP_FORMAT("Server up {} days, {:02}:{:02}:{:02}",
				up / 86400, (up / 3600) % 24, (up / 60) % 60, up % 60));
		}
		break;

		default:
		break;
	}

	stats.AddRow(219, statschar, "End of /STATS report");
	ServerInstance->SNO.WriteToSnoMask('t', "{} '{}' requested by {} ({}@{})", (IS_LOCAL(user) ? "Stats" : "Remote stats"),
		statschar, user->nick, user->GetRealUser(), user->GetRealHost());
}

CmdResult CommandStats::Handle(User* user, const Params& parameters)
{
	if (parameters.size() > 1 && !irc::equals(parameters[1], ServerInstance->Config->ServerName))
	{
		// Give extra penalty if a non-oper does /STATS <remoteserver>
		LocalUser* localuser = IS_LOCAL(user);
		if ((localuser) && (!user->IsOper()))
			localuser->CommandFloodPenalty += 2000;
		return CmdResult::SUCCESS;
	}

	Stats::Context stats(statstags, user, parameters[0][0]);
	DoStats(stats);

	for (const auto& row : stats.GetRows())
		user->WriteRemoteNumeric(row);

	return CmdResult::SUCCESS;
}

class CoreModStats final
	: public Module
{
private:
	CommandStats cmd;

public:
	CoreModStats()
		: Module(VF_CORE | VF_VENDOR, "Provides the STATS command")
		, cmd(this)
	{
	}

	void init() override
	{
		ServerInstance->SNO.EnableSnomask('t', "STATS");
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& security = ServerInstance->Config->ConfValue("security");
		cmd.userstats = security->getString("userstats", "Pu");
	}

};

MODULE_INIT(CoreModStats)
