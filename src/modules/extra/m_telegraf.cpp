/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
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

/// $ModDesc: Provides IRCd metrics to a locally running Telegraf instance.
/// $ModAuthor: linuxdaemon
/// $ModAuthorMail: linuxdaemon@snoonet.org
/// $ModDepends: core 3

/*
 * Gathers metrics to send to a local telegraf instance, connecting on a configurable port
 *
 * Flow:
 * 		Module init
 * 			Create Timer
 * 			Create AtomicAction
 * 			Register Timer
 *
 * 		From Loop:
 * 			LoopLagTimer::Tick
 * 			OnBackgroundtimer (~every 5 secs)
 * 			Socket reads/module calls
 * 			LoopAction::Call
 *
 * 	Data fields can be added in TelegrafSocket::SendMetrics
 *
 * 	Config:
 * 		<module name="telegraf">
 * 		<telegraf
 * 			# Port Telegraf is listening on
 * 			port="8094"
 * 			Whether to announce the start and stop of metrics with a snotice
 * 			silent="false"
 * 			How often to attempt to reconnect to Telegraf after losing connection
 * 			reconnect="60">
 */

#include "inspircd.h"

static const std::string cmd_actions[] = {"start", "stop", "restart", "status", "sample"};

struct Metrics
{
	typedef std::vector<std::clock_t> LoopTimes;
	std::clock_t lastLoopTime;
	LoopTimes loopTimes;

	Metrics() : lastLoopTime(0)
	{
	}

	virtual ~Metrics()
	{
	}

	void clear()
	{
		loopTimes.clear();
		lastLoopTime = 0;
	}

	void addLoopTime(const std::clock_t t)
	{
		loopTimes.push_back(t - lastLoopTime);
	}

	std::clock_t getAverageLoopTime()
	{
		if (loopTimes.empty())
			return 0;
		std::clock_t total = 0;
		for (LoopTimes::size_type i = 0; i < loopTimes.size(); ++i)
			total += loopTimes[i];
		return total / loopTimes.size();
	}
};

struct TelegrafLine
{
	std::string name;
	std::map<std::string, std::string> tags;
	std::map<std::string, std::string> fields;

	TelegrafLine()
	{
	}

	virtual ~TelegrafLine()
	{
	}

	void clear()
	{
		name.clear();
		tags.clear();
		fields.clear();
	}

	std::string escapeTag(const std::string &in)
	{
		std::string out;
		for (std::string::const_iterator i = in.begin(); i != in.end(); i++)
		{
			char c = *i;
			switch (c)
			{
				case ',':
				case ' ':
				case '=':
				case '\\':
					out += '\\';
					break;
			}
			out += c;
		}
		return out;
	}

	std::string escapeValue(const std::string &in)
	{
		std::string out;
		for (std::string::const_iterator i = in.begin(); i != in.end(); i++)
		{
			switch (*i)
			{
				case '"':
				case '\\':
					out += '\\';
					break;
			}
			out += *i;
		}
		return out;
	}

	std::string format()
	{
		std::string out(name);
		for (std::map<std::string, std::string>::const_iterator i = tags.begin(); i != tags.end(); i++)
		{
			out += "," + escapeTag(i->first) + "=" + escapeTag(i->second);
		}
		bool first = true;
		for (std::map<std::string, std::string>::const_iterator i = fields.begin(); i != fields.end(); i++)
		{
			if (first)
			{
				out += " ";
				first = false;
			}
			else
			{
				out += ",";
			}
			out += escapeTag(i->first) + "=" + escapeValue(i->second);
		}
		return out + "\n";
	}
};

class TelegrafModule;

struct LoopAction : public ActionBase
{
	TelegrafModule *creator;

	LoopAction(TelegrafModule *m) : creator(m)
	{
	}

	void Call() CXX11_OVERRIDE;
};

struct LoopLagTimer : public Timer
{
	TelegrafModule *creator;

	LoopLagTimer(TelegrafModule *m) : Timer(0, true), creator(m)
	{
	}

	bool Tick(time_t) CXX11_OVERRIDE;
};

class TelegrafSocket : public BufferedSocket
{
	TelegrafModule *creator;

 public:
	TelegrafSocket(TelegrafModule *m, irc::sockets::sockaddrs connsa) : creator(m)
	{
		irc::sockets::sockaddrs bindsa;
		DoConnect(connsa, bindsa, 60);
	}

	void OnError(BufferedSocketError) CXX11_OVERRIDE;

	void OnDataReady() CXX11_OVERRIDE
	{
		recvq.clear();
	}

	void SendMetrics();

	TelegrafLine GetMetrics();
};

class TelegrafCommand : public Command
{
	std::set<std::string> actions;

 public:
	TelegrafCommand(Module *parent)
			: Command(parent, "TELEGRAF", 1),
			  actions(cmd_actions, cmd_actions + sizeof(cmd_actions) / sizeof(cmd_actions[0]))
	{
		syntax = "{start|stop|restart|status} [<servername>]";
		flags_needed = 'o';
	}

	RouteDescriptor GetRouting(User* user, const CommandBase::Params& parameters) CXX11_OVERRIDE
	{
		if (parameters.size() > 1)
			return ROUTE_OPT_BCAST;
		return ROUTE_LOCALONLY;
	}

	CmdResult Handle(User* user, const CommandBase::Params& parameters) CXX11_OVERRIDE;
};

class TelegrafModule : public Module
{
 public:
	Metrics metrics;

 private:
	bool shouldReconnect;
	bool silent;
	irc::sockets::sockaddrs connsa;
	long reconnectTimeout;
	time_t lastReconnect;
	LoopLagTimer *timer;
	LoopAction *action;
	TelegrafSocket *tSock;
	TelegrafCommand cmd;

	friend class TelegrafCommand;

 public:
	TelegrafModule()
			: shouldReconnect(false)
			, lastReconnect(0)
			, timer(NULL)
			, action(NULL)
			, tSock(NULL)
			, cmd(this)
	{
	}

	void init() CXX11_OVERRIDE
	{
		timer = new LoopLagTimer(this);
		action = new LoopAction(this);
		ServerInstance->Timers.AddTimer(timer);
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag *tag = ServerInstance->Config->ConfValue("telegraf");
		silent = tag->getBool("silent");
		reconnectTimeout = tag->getInt("reconnect", 60, 5);
		irc::sockets::sockaddrs newsa;
		irc::sockets::aptosa("127.0.0.1", tag->getUInt("port", 8094, 1, 65535), newsa);
		if (connsa != newsa)
		{
			if (tSock)
			{
				StopMetrics();
			}

			std::swap(connsa, newsa);
			StartMetrics();
		}
	}

	void OnBackgroundTimer(time_t curtime) CXX11_OVERRIDE
	{
		if (shouldReconnect && !tSock)
		{
			if ((curtime - lastReconnect) >= reconnectTimeout)
			{
				lastReconnect = curtime;
				shouldReconnect = false;
				StartMetrics(true);
			}
		}
		else if (tSock && tSock->HasFd())
		{
			tSock->SendMetrics();
		}
	}

	void LoopTick(bool first)
	{
		if (!tSock)
			return;

		if (first)
		{
			// Triggered from the timer
			metrics.lastLoopTime = std::clock();
			ServerInstance->AtomicActions.AddAction(action);
		}
		else if (metrics.lastLoopTime)
		{
			// Triggered from the atomic call
			metrics.addLoopTime(std::clock());
		}
	}

	void StartMetrics(bool restarted = false)
	{
		tSock = new TelegrafSocket(this, connsa);
		if (!silent)
			ServerInstance->SNO->WriteGlobalSno('a', "METRICS: Telegraf metrics %sstarted.", restarted ? "re" : "");
	}

	void StopMetrics(bool error = false)
	{
		ServerInstance->GlobalCulls.AddItem(tSock);
		if (!silent)
		{
			if (!error)
			{
				ServerInstance->SNO->WriteGlobalSno('a', "METRICS: Telegraf metrics stopped.");
			}
			else
			{
				const char* errmsg = tSock ? tSock->getError().c_str() : "unknown error";
				ServerInstance->SNO->WriteGlobalSno('a', "METRICS: Socket error occurred: %s", errmsg);
			}
		}
		tSock = NULL;
		metrics.clear();
	}

	void SocketError(BufferedSocketError e)
	{
		StopMetrics(true);
		if (reconnectTimeout)
			shouldReconnect = true;
	}

	CullResult cull() CXX11_OVERRIDE
	{
		if (action)
			ServerInstance->GlobalCulls.AddItem(action);
		if (timer)
			ServerInstance->Timers.DelTimer(timer);
		if (tSock)
			StopMetrics();
		return Module::cull();
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Reports ircd stats to a locally running Telegraf instance");
	}
};

bool LoopLagTimer::Tick(time_t)
{
	creator->LoopTick(true);
	return true;
}

void LoopAction::Call()
{
	creator->LoopTick(false);
}

CmdResult TelegrafCommand::Handle(User* user, const CommandBase::Params& parameters)
{
	if (actions.find(parameters[0]) == actions.end())
	{
		if (IS_LOCAL(user))
		{
			user->WriteNumeric(RPL_SYNTAX, "%s :SYNTAX %s %s", user->nick.c_str(), name.c_str(), syntax.c_str());
		}
		return CMD_FAILURE;
	}

	if (parameters.size() > 1 && !InspIRCd::Match(ServerInstance->Config->ServerName, parameters[1]))
	{
		// Route the command only to the remote server specified
		return CMD_SUCCESS;
	}

	TelegrafModule *mod = static_cast<TelegrafModule *> (static_cast<Module *> (creator));
	std::vector<std::string> messages;
	std::string message;
	if (parameters[0] == "start")
	{
		if (!mod->tSock)
		{
			mod->StartMetrics();
			messages.push_back("Telegraf metrics started");
		}
		else
		{
			messages.push_back("Telegraf metrics already running");
		}
	}
	else if (parameters[0] == "stop")
	{
		if (mod->tSock)
		{
			mod->shouldReconnect = false;
			mod->StopMetrics();
			messages.push_back("Telegraf metrics stopped");
		}
		else
		{
			messages.push_back("Telegraf metrics not running");
		}
	}
	else if (parameters[0] == "restart")
	{
		if (mod->tSock)
		{
			mod->StopMetrics();
			mod->StartMetrics(true);
			messages.push_back("Telegraf metrics restarted");
		}
		else
		{
			messages.push_back("Telegraf metrics not running");
		}
	}
	else if (parameters[0] == "status")
	{
		if (mod->tSock)
		{
			messages.push_back("Telegraf metrics running");
		}
		else
		{
			messages.push_back("Telegraf metrics not running");
		}
	}
	else if (parameters[0] == "sample")
	{
		if (mod->tSock)
		{
			TelegrafLine line = mod->tSock->GetMetrics();
			messages.push_back("Name: " + line.name);
			messages.push_back("Tags:");
			for (std::map<std::string, std::string>::const_iterator i = line.tags.begin(); i != line.tags.end(); ++i)
			{
				messages.push_back("    " + i->first + "=" + i->second);
			}
			messages.push_back("Values:");
			for (std::map<std::string, std::string>::const_iterator i = line.fields.begin();
				 i != line.fields.end(); ++i)
			{
				messages.push_back("    " + i->first + "=" + i->second);
			}
			messages.push_back("End of metrics");
		}
		else
		{
			messages.push_back("Telegraf metrics don't appear to be running");
		}
	}
	else
	{
		return CMD_FAILURE;
	}

	for (std::vector<std::string>::size_type i = 0; i < messages.size(); ++i)
	{
		if (parameters.size() > 1)
			user->WriteNotice(InspIRCd::Format("*** From %s: %s", ServerInstance->Config->ServerName.c_str(), messages[i].c_str()));
		else
			user->WriteNotice("*** " + messages[i]);
	}

	return CMD_SUCCESS;
}

void TelegrafSocket::OnError(BufferedSocketError e)
{
	if (creator)
		creator->SocketError(e);
}

void TelegrafSocket::SendMetrics()
{
	ServerInstance->Logs->Log("TELEGRAF", LOG_DEBUG, "Sending Telegraf Metrics..");
	TelegrafLine line = GetMetrics();
	creator->metrics.loopTimes.clear();
	creator->metrics.loopTimes.reserve(10);
	std::string out(line.format());
	WriteData(out);
	ServerInstance->Logs->Log("TELEGRAF", LOG_DEBUG, "Sent Telegraf metrics: %s", out.c_str());
}

TelegrafLine TelegrafSocket::GetMetrics()
{
	TelegrafLine line;
	line.name = "ircd";
	line.tags["server"] = ServerInstance->Config->ServerName;
	line.fields["users"] = ConvToStr(ServerInstance->Users->LocalUserCount());
	float bits_in, bits_out, bits_total;
	SocketEngine::GetStats().GetBandwidth(bits_in, bits_out, bits_total);
	line.fields["rate_in"] = ConvToStr(bits_in);
	line.fields["rate_out"] = ConvToStr(bits_out);
	line.fields["rate_total"] = ConvToStr(bits_total);
	line.fields["data_sent"] = ConvToStr(ServerInstance->stats.Sent);
	line.fields["data_recv"] = ConvToStr(ServerInstance->stats.Recv);
	line.fields["dns"] = ConvToStr(ServerInstance->stats.Dns);
	line.fields["dns_good"] = ConvToStr(ServerInstance->stats.DnsGood);
	line.fields["dns_bad"] = ConvToStr(ServerInstance->stats.DnsBad);
	line.fields["sock_accepts"] = ConvToStr(ServerInstance->stats.Accept);
	line.fields["sock_refused"] = ConvToStr(ServerInstance->stats.Refused);
	line.fields["connects"] = ConvToStr(ServerInstance->stats.Connects);
	line.fields["nick_collisions"] = ConvToStr(ServerInstance->stats.Collisions);
	line.fields["cmd_unknown"] = ConvToStr(ServerInstance->stats.Unknown);
	line.fields["sockets"] = ConvToStr(SocketEngine::GetUsedFds());
	line.fields["main_loop_time"] = ConvToStr(creator->metrics.getAverageLoopTime());
	return line;
}

MODULE_INIT(TelegrafModule)
