/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 Chris Novakovic
 *   Copyright (C) 2013, 2017-2025 Sadie Powell <sadie@sadiepowell.dev>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012-2014, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012-2013 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Ariadne Conill <ariadne@dereferenced.org>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <om@inspircd.org>
 *   Copyright (C) 2005-2009 Craig Edwards <brain@inspircd.org>
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


#include <filesystem>
#include <fstream>
#include <iostream>

#include <fmt/color.h>
#include <lyra/lyra.hpp>

#include "inspircd.h"
#include "timeutils.h"
#include "xline.h"

#ifndef _WIN32
# include <fcntl.h>
# include <grp.h>
# include <pwd.h>
# include <sys/resource.h>
# include <unistd.h>
#else
# define STDIN_FILENO 0
# define STDOUT_FILENO 1
# define STDERR_FILENO 2
# include <process.h>
#endif

InspIRCd* ServerInstance = nullptr;

namespace
{
	[[noreturn]]
	void VoidSignalHandler(int);

	// Warns a user running as root that they probably shouldn't.
	void CheckRoot()
	{
#ifndef _WIN32
		if (getegid() != 0 && geteuid() != 0)
			return;

		const auto has_tty = isatty(fileno(stdout));
		if (has_tty)
			fmt::print("{} ", fmt::styled("Warning!", fmt::emphasis::bold | fmt::fg(fmt::terminal_color::yellow)));
		else
			fmt::print("{} ", fmt::styled("Error!", fmt::emphasis::bold | fmt::fg(fmt::terminal_color::red)));

		fmt::println("You have started as root. Running as root is generally not required");
		fmt::println("and may allow an attacker to gain access to your system if they find a way to");
		fmt::println("exploit your IRC server.");
		fmt::println("");
		if (has_tty)
		{
			fmt::println("InspIRCd will start in 30 seconds. If you are sure that you need to run as root");
			fmt::println("then you can pass the {} option to disable this wait.", fmt::styled("--runasroot", fmt::emphasis::bold));
			sleep(30);
		}
		else
		{
			fmt::println("If you are sure that you need to run as root then you can pass the {}", fmt::styled("--runasroot", fmt::emphasis::bold));
			fmt::println("option to disable this error.");
			ServerInstance->Exit(EXIT_FAILURE);
		}
#endif
	}

	// Collects performance statistics for the STATS command.
	void CollectStats()
	{
#ifndef _WIN32
		static rusage ru;
		if (getrusage(RUSAGE_SELF, &ru) == -1)
			return; // Should never happen.

		ServerInstance->Stats.LastSampled.tv_sec = ServerInstance->Time();
		ServerInstance->Stats.LastSampled.tv_nsec = ServerInstance->Time_ns();
		ServerInstance->Stats.LastCPU = ru.ru_utime;
#else
		if (!QueryPerformanceCounter(&ServerInstance->Stats.LastCPU))
			return; // Should never happen.

		FILETIME CreationTime;
		FILETIME ExitTime;
		FILETIME KernelTime;
		FILETIME UserTime;
		GetProcessTimes(GetCurrentProcess(), &CreationTime, &ExitTime, &KernelTime, &UserTime);

		ServerInstance->Stats.LastSampled.dwHighDateTime = KernelTime.dwHighDateTime + UserTime.dwHighDateTime;
		ServerInstance->Stats.LastSampled.dwLowDateTime = KernelTime.dwLowDateTime + UserTime.dwLowDateTime;
#endif
	}

	// Checks whether the server clock has skipped too much and warn about it if it has.
	void CheckTimeSkip(time_t oldtime, time_t newtime)
	{
		if (!ServerInstance->Config->TimeSkipWarn)
			return;

		time_t timediff = newtime - oldtime;

		if (timediff > ServerInstance->Config->TimeSkipWarn)
		{
			ServerInstance->SNO.WriteToSnoMask('a', "\002Performance warning!\002 Server clock jumped forwards by {} (from {} to {})!",
				Duration::ToLongString(timediff), Time::ToString(oldtime), Time::ToString(newtime));
		}

		else if (timediff < -ServerInstance->Config->TimeSkipWarn)
		{
			ServerInstance->SNO.WriteToSnoMask('a', "\002Performance warning!\002 Server clock jumped backwards by {} (from {} to {})!",
				Duration::ToLongString(std::abs(timediff)), Time::ToString(oldtime), Time::ToString(newtime));
		}
	}

	// Drops to the unprivileged user/group specified in <security:runas{user,group}>.
	void DropRoot()
	{
#ifndef _WIN32
		const auto& security = ServerInstance->Config->ConfValue("security");

		const std::string SetGroup = security->getString("runasgroup");
		if (!SetGroup.empty())
		{
			errno = 0;
			if (setgroups(0, nullptr) == -1)
			{
				InspIRCd::QuickExit(EXIT_FAILURE, FMT::format("Unable to drop supplementary groups: {}",
					strerror(errno)), "STARTUP");
			}

			struct group* g = getgrnam(SetGroup.c_str());
			if (!g)
			{
				InspIRCd::QuickExit(EXIT_FAILURE, FMT::format("Unable to find the {} group to switch to: {}",
					SetGroup, strerror(errno)), "STARTUP");
			}

			if (setgid(g->gr_gid) == -1)
			{
				InspIRCd::QuickExit(EXIT_FAILURE, FMT::format("Unable to switch to the {} group: {}",
					SetGroup, strerror(errno)), "STARTUP");
			}
		}

		const std::string SetUser = security->getString("runasuser");
		if (!SetUser.empty())
		{
			errno = 0;
			struct passwd* u = getpwnam(SetUser.c_str());
			if (!u)
			{
				InspIRCd::QuickExit(EXIT_FAILURE, FMT::format("Unable to find the {} user to switch to: {}",
					SetUser, strerror(errno)), "STARTUP");
			}

			if (setuid(u->pw_uid) == -1)
			{
				InspIRCd::QuickExit(EXIT_FAILURE, FMT::format("Unable to switch to the {} user: {}",
					SetUser, strerror(errno)), "STARTUP");
			}
		}
#endif
	}

	// Expands a path relative to the current working directory.
	std::string ExpandPath(const char* path)
	{
#ifdef _WIN32
		TCHAR configPath[MAX_PATH + 1];
		if (GetFullPathName(path, MAX_PATH, configPath, nullptr) > 0)
			return configPath;
#else
		char configPath[PATH_MAX + 1];
		if (realpath(path, configPath))
			return configPath;
#endif
		return path;
	}

	// Attempts to fork into the background.
	void ForkIntoBackground()
	{
#ifndef _WIN32
		// We use VoidSignalHandler whilst forking to avoid breaking daemon scripts
		// if the parent process exits with SIGTERM (15) instead of EXIT_SUCCESS (0).
		signal(SIGTERM, VoidSignalHandler);

		errno = 0;
		int childpid = fork();
		if (childpid < 0)
		{
			ServerInstance->Exit(EXIT_FAILURE, FMT::format("Unable to fork into the background: {}",
				strerror(errno)), "STARTUP");
		}
		else if (childpid > 0)
		{
			// Wait until the child process kills the parent so that the shell prompt
			// doesnt display over the output. Sending a kill with a signal of 0 just
			// checks that the child pid is still running. If it is not then an error
			// happened and the parent should exit.
			while (kill(childpid, 0) != -1)
				sleep(1);
			exit(EXIT_FAILURE);
		}
		else
		{
			setsid();
			signal(SIGTERM, InspIRCd::SetSignal);
			SocketEngine::RecoverFromFork();
		}
#endif
	}

	// Increase the size of a core dump file to improve debugging problems.
	void IncreaseCoreDumpSize()
	{
#ifndef _WIN32
		errno = 0;
		rlimit rl;
		if (getrlimit(RLIMIT_CORE, &rl) == -1)
		{
			ServerInstance->Logs.Warning("STARTUP", "Unable to increase core dump size: getrlimit(RLIMIT_CORE) failed: {}", strerror(errno));
			return;
		}

		rl.rlim_cur = rl.rlim_max;
		if (setrlimit(RLIMIT_CORE, &rl) == -1)
			ServerInstance->Logs.Warning("STARTUP", "Unable to increase core dump size: setrlimit(RLIMIT_CORE) failed: {}", strerror(errno));
#endif
	}

	// Parses the command line options.
	void ParseOptions()
	{
		std::string config = ServerInstance->Config->Paths.PrependConfig("inspircd.conf");
		bool do_debug = false;
		bool do_help = false;
		bool do_nofork = false;
		bool do_nolog = false;
		bool do_nopid = false;
		bool do_protocoldebug = false;
		bool do_runasroot = false;
		bool do_version = false;

		auto cli = lyra::cli()
			| lyra::opt(config, "FILE")
				["-c"]["--config"]
				("The location of the main config file.")
			| lyra::opt(do_debug)
				["-d"]["--debug"]
				("Start in debug mode.")
			| lyra::opt(do_nofork)
				["-F"]["--nofork"]
				("Disable forking into the background.")
			| lyra::opt(do_help)
				["-h"]["--help"]
				("Show help and exit.")
			| lyra::opt(do_nolog)
				["-L"]["--nolog"]
				("Disable writing logs to disk.")
			| lyra::opt(do_protocoldebug)
				["-p"]["--protocoldebug"]
				("Start in protocol debug mode.")
			| lyra::opt(do_nopid)
				["-P"]["--nopid"]
				("Disable writing the pid file.")
			| lyra::opt(do_runasroot)
				["-r"]["--runasroot"]
				("Allow starting as root (not recommended).")
			| lyra::opt(do_version)
				["-v"]["--version"]
				("Show version and exit.");

		auto result = cli.parse({ServerInstance->CommandLine.argc, ServerInstance->CommandLine.argv});
		if (!result)
			ServerInstance->Exit(EXIT_FAILURE, FMT::format("An invalid option was passed on the command line: {}", result.message()));

		if (do_help)
		{
			std::cout << cli << std::endl;
			ServerInstance->Exit(EXIT_SUCCESS);
		}

		if (do_version)
		{
			fmt::println(INSPIRCD_VERSION);
			ServerInstance->Exit(EXIT_SUCCESS);
		}

		// Store the relevant parsed arguments
		if (!config.empty())
			ServerInstance->ConfigFileName = ExpandPath(config.c_str());
		ServerInstance->CommandLine.forcedebug = do_debug || do_protocoldebug;
		ServerInstance->CommandLine.forceprotodebug = do_protocoldebug;
		ServerInstance->CommandLine.nofork = ServerInstance->CommandLine.forcedebug || do_nofork;
		ServerInstance->CommandLine.runasroot = do_runasroot;
		ServerInstance->CommandLine.writelog = !do_nolog;
		ServerInstance->CommandLine.writepid = !do_nopid;
	}

	// Sets handlers for various process signals.
	void SetSignals()
	{
#ifndef _WIN32
		signal(SIGALRM, SIG_IGN);
		signal(SIGCHLD, SIG_IGN);
		signal(SIGHUP, InspIRCd::SetSignal);
		signal(SIGPIPE, SIG_IGN);
		signal(SIGUSR1, SIG_IGN);
		signal(SIGUSR2, SIG_IGN);
		signal(SIGXFSZ, SIG_IGN);
#endif
#if defined(SIGRTMIN) && defined(SIGRTMAX)
		for (auto rtsig = SIGRTMIN; rtsig <= SIGRTMAX; ++rtsig)
			signal(rtsig, SIG_IGN);
#endif
		signal(SIGTERM, InspIRCd::SetSignal);
	}

	void TryBindPorts()
	{
		FailedPortList pl;
		ServerInstance->BindPorts(pl);

		if (!pl.empty())
		{
			fmt::println("{} Some of your listeners failed to bind:", fmt::styled("Warning!", fmt::emphasis::bold | fmt::fg(fmt::terminal_color::yellow)));
			fmt::println("");

			for (const auto& fp : pl)
			{
				if (fp.sa.family() != AF_UNSPEC)
					fmt::print("{}: ", fmt::styled(fp.sa.str(), fmt::emphasis::bold));

				fmt::println("{}.", fp.error);
				fmt::println("  Created from <bind> tag at {}", fp.tag->source.str());
				fmt::println("");
			}

			fmt::println("{}", fmt::styled("Hints:", fmt::emphasis::bold));
			fmt::println("- For TCP/IP listeners try using a public IP address in <bind:address> instead");
			fmt::println("  of * or leaving it blank.");
			fmt::println("- For UNIX socket listeners try enabling <bind:replace> to replace old sockets.");
			fmt::println("");
		}
	}

	// Required for returning the proper value of EXIT_SUCCESS for the parent process.
	void VoidSignalHandler(int)
	{
		exit(EXIT_SUCCESS);
	}
}

void InspIRCd::Cleanup(const std::string &reason)
{
	// Close all listening sockets.
	for (auto& port : this->Ports)
		insp::cull_delete_zero(port);
	this->Ports.clear();

	// Disconnect all local users.
	const auto& list = this->Users.GetLocalUsers();
	while (!list.empty())
		ServerInstance->Users.QuitUser(list.front(), reason);

	// Ensure any users we just quit are culled.
	GlobalCulls.Apply();

	// Unload all modules.
	this->Modules.UnloadAll();

	// Clean up the server objects.
	insp::cull_delete_zero(this->FakeClient);
	insp::cull_delete_zero(this->LocalServer);
	insp::delete_zero(this->XLines);
	this->Config.reset(nullptr);

	// Deinit the remaining subsystems.
	SocketEngine::Deinit();
	Logs.CloseLogs();

	// Finally, delete the server.
	insp::delete_zero(ServerInstance);
}

void InspIRCd::QuickExit(int status, const std::string& reason, const std::string& logtype)
{
	const auto should_print = isatty(fileno(stdout)) && status != EXIT_SUCCESS;
	if (should_print)
		fmt::println("");

	if (!reason.empty())
	{
		if (ServerInstance && !logtype.empty())
			ServerInstance->Logs.Critical(logtype, reason);

		if (should_print)
		{
			fmt::print("{} ", fmt::styled("Error!", fmt::emphasis::bold | fmt::fg(fmt::terminal_color::red)));
			fmt::println("{}.", reason);
		}
	}

	if (should_print)
		fmt::println("Exiting with code {}.",status);

#ifdef _WIN32
	SetServiceStopped(status);
#endif
	exit(status);
}

void InspIRCd::Exit(int status, const std::string& reason, const std::string& logtype)
{
	// Tell modules that we're shutting down.
	const auto quitmsg = reason.empty() ? "Server shutting down" : reason;
	FOREACH_MOD(OnShutdown, (quitmsg, false));

	// Clean up all subsystems and then exit.
	this->Cleanup(quitmsg);
	InspIRCd::QuickExit(status, reason, logtype);
}

std::string InspIRCd::Restart(const std::string& reason, const std::string& logtype)
{
	// Tell modules that we're restarting.
	const auto quitmsg = reason.empty() ? "Server restarting" : reason;
	FOREACH_MOD(OnShutdown, (quitmsg, true));

	// Clean up all subsystems.
	this->Cleanup(quitmsg);

	// HACK: Because execvp does not close any remaining open file descriptors
	// we need ensure they are closed. On modern Linux we can do this with the
	// close_range function but on older systems we need to fall back to setting
	// the FD_CLOEXEC flag on all possible file descriptors. This is very slow
	// but there's not much else we can do because we don't control the fd
	// creation of all of our dependencies.
	const auto first_fd = fileno(stderr) + 1;
#if defined HAS_CLOSE_RANGE && defined CLOSE_RANGE_CLOEXEC
	close_range(first_fd, ~0, CLOSE_RANGE_CLOEXEC);
#elif defined FD_CLOEXEC
	for (auto fd = SocketEngine::GetMaxFds(); fd >= size_t(first_fd); ++fd)
	{
		const auto flags = fcntl(int(fd), F_GETFD);
		if (flags != 1)
			fcntl(int(fd), F_SETFD, flags | FD_CLOEXEC);
	}
#endif

	const auto should_print = isatty(fileno(stdout));
	if (should_print)
		fmt::println("");

	if (!reason.empty())
	{
		if (!logtype.empty())
			ServerInstance->Logs.Critical(logtype, reason);

		if (should_print)
			fmt::println("Server is restarting: {}.", reason);
	}

	execvp(this->CommandLine.argv[0], this->CommandLine.argv);

	const std::string error = strerror(errno);
	if (should_print)
	{
		if (!logtype.empty())
			ServerInstance->Logs.Critical(logtype, "Server restart failed: {}", error);
		fmt::println("Server restart failed: {}.", error);
	}

	return error;
}

void InspIRCd::WritePID()
{
	if (!CommandLine.writepid)
	{
		this->Logs.Normal("STARTUP", "--nopid specified on command line; PID file not written.");
		return;
	}

	const std::string pidfile = ServerInstance->Config->ConfValue("pid")->getString("file", "inspircd.pid", 1);
	std::ofstream outfile(ServerInstance->Config->Paths.PrependRuntime(pidfile));
	if (outfile.is_open())
	{
		outfile << getpid();
		outfile.close();
	}
	else
	{
		fmt::println("Failed to write PID-file '{}', exiting.", pidfile);
		this->Logs.Critical("STARTUP", "Failed to write PID-file '{}', exiting.", pidfile);
		Exit(EXIT_FAILURE);
	}
}

InspIRCd::InspIRCd(int argc, char** argv)
	: StartTime(time(NULL))
{
	ServerInstance = this;

	UpdateTime();
	IncreaseCoreDumpSize();
	SocketEngine::Init();

	this->Config = std::make_unique<ServerConfig>();
	dynamic_reference_base::reset_all();
	this->XLines = new XLineManager;

	this->CommandLine.argv = argv;
	this->CommandLine.argc = argc;
	ParseOptions();

	Modules.AddService(
		rfcevents.numeric, rfcevents.join, rfcevents.part, rfcevents.kick, rfcevents.quit,
		rfcevents.nick, rfcevents.mode, rfcevents.topic, rfcevents.privmsg, rfcevents.invite,
		rfcevents.ping, rfcevents.pong, rfcevents.reply, rfcevents.error
	);

	fmt::println("{}{} - A high-performance IRCv3 server for UNIX-like and Windows systems.",
		fmt::styled("Insp", fmt::emphasis::bold),
		fmt::styled("IRCd", fmt::emphasis::bold | fmt::fg(fmt::terminal_color::red))
	);
	fmt::println("See {} for a list of developers and contributors.", fmt::styled("/INFO", fmt::emphasis::bold));
	fmt::println("");
	fmt::println("{:<15} https://www.inspircd.org", fmt::styled("Website:", fmt::emphasis::bold));
	fmt::println("{:<15} https://docs.inspircd.org", fmt::styled("Documentation:", fmt::emphasis::bold));
	fmt::println("{:<15} https://docs.inspircd.org/support", fmt::styled("Support:", fmt::emphasis::bold));
	fmt::println("");
	fmt::println("InspIRCd is open source and is reliant on your support to stay maintained. Read");
	fmt::println("{} to find out how to become a contributor", fmt::styled("https://docs.inspircd.org/contributing", fmt::emphasis::bold));
	fmt::println("or {} to donate to the project.", fmt::styled("https://www.inspircd.org/donate", fmt::emphasis::bold));
	fmt::println("");

	Logs.RegisterServices();
	if (CommandLine.forcedebug)
		Logs.EnableDebugMode(CommandLine.forceprotodebug);

	std::error_code ec;
	if (!std::filesystem::is_regular_file(ConfigFileName, ec))
	{
		this->Logs.Critical("STARTUP", "Unable to find config file {}", ConfigFileName);
		Exit(EXIT_FAILURE, FMT::format("Unable to find the config file: {}", ConfigFileName));
	}

	SetSignals();
	if (!CommandLine.runasroot)
		CheckRoot();
	if (!CommandLine.nofork)
		ForkIntoBackground();

	/* During startup we read the configuration now, not in
	 * a separate thread
	 */
	this->Config->Read();
	this->Config->Apply(nullptr, "");

	try
	{
		Logs.CloseLogs();
		Logs.OpenLogs(false);
	}
	catch (const CoreException& ex)
	{
		Exit(EXIT_FAILURE, FMT::format("Cannot open log files: {}", ex.GetReason()));
	}

	// We only do this on boot because we might not be able to after dropping root.
	WritePID();

	// If we don't have a SID, generate one based on the server name and the server description
	if (Config->ServerId.empty())
		Config->ServerId = UIDGenerator::GenerateSID(Config->ServerName, Config->ServerDesc);

	// Initialize the UID generator with our sid
	this->UIDGen.init(Config->ServerId);

	// Create the server user for this server
	this->FakeClient = new FakeUser(Config->ServerId, Config->ServerName, Config->ServerDesc);
	this->LocalServer = this->FakeClient->server;

	// This is needed as all new XLines are marked pending until ApplyLines() is called
	this->XLines->ApplyLines();

	TryBindPorts();

	this->Modules.LoadAll();
	try
	{
		// We reopen logs again after modules to allow module loggers to have a chance to register.
		Logs.CloseLogs();
		Logs.OpenLogs(true);
	}
	catch (const CoreException& ex)
	{
		Exit(EXIT_FAILURE, FMT::format("Cannot open log files: {}", ex.GetReason()));
	}

	fmt::println("{} [PID {}] is now running as {} [{}] with {} max open sockets",
		INSPIRCD_VERSION, getpid(), Config->ServerName, Config->ServerId, SocketEngine::GetMaxFds());

#ifndef _WIN32
	if (!CommandLine.nofork)
	{
		if (kill(getppid(), SIGTERM) == -1)
		{
			fmt::println("Error killing parent process: {}", strerror(errno));
			Logs.Warning("STARTUP", "Error killing parent process: {}", strerror(errno));
		}
	}

	/* Explicitly shut down stdio's stdin/stdout/stderr.
	 *
	 * The previous logic here was to only do this if stdio was connected to a controlling
	 * terminal.  However, we must do this always to avoid information leaks and other
	 * problems related to stdio.
	 *
	 * The only exception is if we are in debug mode.
	 *
	 *    -- nenolod
	 */
	if (!CommandLine.nofork)
	{
		int fd = open("/dev/null", O_RDWR);

		fclose(stdin);
		fclose(stderr);
		fclose(stdout);

		if (dup2(fd, STDIN_FILENO) < 0)
			Logs.Warning("STARTUP", "Failed to dup /dev/null to stdin.");
		if (dup2(fd, STDOUT_FILENO) < 0)
			Logs.Warning("STARTUP", "Failed to dup /dev/null to stdout.");
		if (dup2(fd, STDERR_FILENO) < 0)
			Logs.Warning("STARTUP", "Failed to dup /dev/null to stderr.");
		close(fd);
	}
	else
	{
		fflush(stderr);
		fflush(stdout);

		Logs.Normal("STARTUP", "Keeping pseudo-tty open as we are running in the foreground.");
	}
#else
	/* Set win32 service as running, if we are running as a service */
	SetServiceRunning();

	// Handle forking
	if(!CommandLine.nofork)
	{
		FreeConsole();
	}

	QueryPerformanceFrequency(&this->Stats.BootCPU);
#endif

	DropRoot();

	Logs.Normal("STARTUP", "Startup complete as '{}'[{}], {} max open sockets", Config->ServerName,
		Config->ServerId, SocketEngine::GetMaxFds());
}

void InspIRCd::UpdateTime()
{
#if defined HAS_CLOCK_GETTIME
	clock_gettime(CLOCK_REALTIME, &ts);
#elif defined _WIN32
	SYSTEMTIME st;
	GetSystemTime(&st);

	ts.tv_sec = time(nullptr);
	ts.tv_nsec = st.wMilliseconds;
#else
	struct timeval tv;
	gettimeofday(&tv, nullptr);

	ts.tv_sec = tv.tv_sec;
	ts.tv_nsec = tv.tv_usec * 1000;
#endif
}

void InspIRCd::Run()
{
	UpdateTime();
	auto oldtime = Time();

	while (true)
	{
		/* Check if there is a config thread which has finished executing but has not yet been freed */
		if (this->ConfigThread && this->ConfigThread->IsDone())
		{
			/* Rehash has completed */
			this->Logs.Normal("CONFIG", "New configuration has been read, applying...");
			ConfigThread->Stop();
			ConfigThread.reset(nullptr);
		}

		UpdateTime();

		// Normally we want to limit the mainloop to processing data
		// once a second but this can cause problems with testing
		// software like irctest. Don't define this unless you know
		// what you are doing.
#ifndef INSPIRCD_UNLIMITED_MAINLOOP
		if (Time() != oldtime)
#endif
		{
			CollectStats();
			CheckTimeSkip(oldtime, Time());

			oldtime = Time();

			if ((Time() % 3600) == 0)
				FOREACH_MOD(OnGarbageCollect, ());

			Timers.TickTimers();
			Users.DoBackgroundUserStuff();

			if ((Time() % 5) == 0)
			{
				FOREACH_MOD(OnBackgroundTimer, (Time()));
				SNO.FlushSnotices();
			}
		}

		/* Call the socket engine to wait on the active
		 * file descriptors. The socket engine has everything's
		 * descriptors in its list... dns, modules, users,
		 * servers... so its nice and easy, just one call.
		 * This will cause any read or write events to be
		 * dispatched to their handlers.
		 */
		SocketEngine::DispatchTrialWrites();
		SocketEngine::DispatchEvents();

		/* if any users were quit, take them out */
		GlobalCulls.Apply();
		AtomicActions.Run();

		if (lastsignal)
		{
			HandleSignal(lastsignal);
			lastsignal = 0;
		}
	}
}

sig_atomic_t InspIRCd::lastsignal = 0;

void InspIRCd::SetSignal(int signal)
{
	lastsignal = signal;
}

#ifdef _WIN32
int smain(int argc, char** argv)
#else
int main(int argc, char** argv)
#endif
{
	new InspIRCd(argc, argv);
	ServerInstance->Run();
	delete ServerInstance;
	return 0;
}
