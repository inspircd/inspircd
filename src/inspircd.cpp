/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Val Lorentz <progval+git@progval.net>
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018 Chris Novakovic
 *   Copyright (C) 2013, 2017-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012-2014, 2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012-2013 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Ariadne Conill <ariadne@dereferenced.org>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 Oliver Lupton <om@inspircd.org>
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

#include <lyra/lyra.hpp>
#include <rang/rang.hpp>

#include "inspircd.h"
#include "exitcodes.h"
#include "xline.h"

#ifndef _WIN32
# include <fcntl.h>
# include <grp.h>
# include <pwd.h>
# include <sys/resource.h>
#else
# define STDIN_FILENO 0
# define STDOUT_FILENO 1
# define STDERR_FILENO 2
# include <process.h>
#endif

InspIRCd* ServerInstance = nullptr;

/** Separate from the other casemap tables so that code *can* still exclusively rely on RFC casemapping
 * if it must.
 *
 * This is provided as a pointer so that modules can change it to their custom mapping tables,
 * e.g. for national character support.
 */
const unsigned char* national_case_insensitive_map = ascii_case_insensitive_map;

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

		std::cout << rang::style::bold << rang::fg::red << "Warning!" << rang::style::reset << " You have started as root. Running as root is generally not required" << std::endl
			<< "and may allow an attacker to gain access to your system if they find a way to" << std::endl
			<< "exploit your IRC server." << std::endl
			<< std::endl;
		if (isatty(fileno(stdout)))
		{
			std::cout << "InspIRCd will start in 30 seconds. If you are sure that you need to run as root" << std::endl
				<< "then you can pass the " << rang::style::bold << "--runasroot" << rang::style::reset << " option to disable this wait." << std::endl;
			sleep(30);
		}
		else
		{
			std::cout << "If you are sure that you need to run as root then you can pass the " << rang::style::bold << "--runasroot" << rang::style::reset << std::endl
				<< "option to disable this error." << std::endl;
				ServerInstance->Exit(EXIT_STATUS_ROOT);
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

		ServerInstance->stats.LastSampled.tv_sec = ServerInstance->Time();
		ServerInstance->stats.LastSampled.tv_nsec = ServerInstance->Time_ns();
		ServerInstance->stats.LastCPU = ru.ru_utime;
#else
		if (!QueryPerformanceCounter(&ServerInstance->stats.LastSampled))
			return; // Should never happen.

		FILETIME CreationTime;
		FILETIME ExitTime;
		FILETIME KernelTime;
		FILETIME UserTime;
		GetProcessTimes(GetCurrentProcess(), &CreationTime, &ExitTime, &KernelTime, &UserTime);

		ServerInstance->stats.LastCPU.dwHighDateTime = KernelTime.dwHighDateTime + UserTime.dwHighDateTime;
		ServerInstance->stats.LastCPU.dwLowDateTime = KernelTime.dwLowDateTime + UserTime.dwLowDateTime;
#endif
	}

	// Checks whether the server clock has skipped too much and warn about it if it has.
	void CheckTimeSkip(time_t oldtime, time_t newtime)
	{
		if (!ServerInstance->Config->TimeSkipWarn)
			return;

		time_t timediff = newtime - oldtime;

		if (timediff > ServerInstance->Config->TimeSkipWarn)
			ServerInstance->SNO.WriteToSnoMask('a', "\002Performance warning!\002 Server clock jumped forwards by {} seconds!", timediff);

		else if (timediff < -ServerInstance->Config->TimeSkipWarn)
			ServerInstance->SNO.WriteToSnoMask('a', "\002Performance warning!\002 Server clock jumped backwards by {} seconds!", labs(timediff));
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
				ServerInstance->Logs.Error("STARTUP", "setgroups() failed (wtf?): {}", strerror(errno));
				InspIRCd::QuickExit(EXIT_STATUS_CONFIG);
			}

			struct group* g = getgrnam(SetGroup.c_str());
			if (!g)
			{
				ServerInstance->Logs.Error("STARTUP", "getgrnam({}) failed (wrong group?): {}", SetGroup, strerror(errno));
				InspIRCd::QuickExit(EXIT_STATUS_CONFIG);
			}

			if (setgid(g->gr_gid) == -1)
			{
				ServerInstance->Logs.Error("STARTUP", "setgid({}) failed (wrong group?): {}", g->gr_gid, strerror(errno));
				InspIRCd::QuickExit(EXIT_STATUS_CONFIG);
			}
		}

		const std::string SetUser = security->getString("runasuser");
		if (!SetUser.empty())
		{
			errno = 0;
			struct passwd* u = getpwnam(SetUser.c_str());
			if (!u)
			{
				ServerInstance->Logs.Error("STARTUP", "getpwnam({}) failed (wrong user?): {}", SetUser, strerror(errno));
				InspIRCd::QuickExit(EXIT_STATUS_CONFIG);
			}

			if (setuid(u->pw_uid) == -1)
			{
				ServerInstance->Logs.Error("STARTUP", "setuid({}) failed (wrong user?): {}", u->pw_uid, strerror(errno));
				InspIRCd::QuickExit(EXIT_STATUS_CONFIG);
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

	// Locates a config file on the file system.
	bool FindConfigFile(std::string& path)
	{
		std::error_code ec;
		if (std::filesystem::is_regular_file(path, ec))
			return true;

#ifdef _WIN32
		// Windows hides file extensions by default so try appending .txt to the path
		// to help users who have that feature enabled and can't create .conf files.
		const std::string txtpath = path + ".txt";
		if (std::filesystem::is_regular_file(txtpath, ec))
		{
			path.assign(txtpath);
			return true;
		}
#endif
		return false;
	}

	// Attempts to fork into the background.
	void ForkIntoBackground()
	{
#ifndef _WIN32
		// We use VoidSignalHandler whilst forking to avoid breaking daemon scripts
		// if the parent process exits with SIGTERM (15) instead of EXIT_STATUS_NOERROR (0).
		signal(SIGTERM, VoidSignalHandler);

		errno = 0;
		int childpid = fork();
		if (childpid < 0)
		{
			ServerInstance->Logs.Error("STARTUP", "fork() failed: {}", strerror(errno));
			std::cout << rang::style::bold << rang::fg::red << "Error:" << rang::style::reset << " unable to fork into background: " << strerror(errno);
			ServerInstance->Exit(EXIT_STATUS_FORK);
		}
		else if (childpid > 0)
		{
			// Wait until the child process kills the parent so that the shell prompt
			// doesnt display over the output. Sending a kill with a signal of 0 just
			// checks that the child pid is still running. If it is not then an error
			// happened and the parent should exit.
			while (kill(childpid, 0) != -1)
				sleep(1);
			InspIRCd::QuickExit(EXIT_STATUS_NOERROR);
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
		std::string config;
		bool do_debug = false;
		bool do_help = false;
		bool do_nofork = false;
		bool do_nolog = false;
		bool do_nopid = false;
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
			| lyra::opt(do_nopid)
				["-P"]["--nopid"]
				("Disable writing the pid file.")
			| lyra::opt(do_runasroot)
				["-r"]["--runasroot"]
				("Allow starting as root (not recommended).")
			| lyra::opt(do_version)
				["-v"]["--version"]
				("Show version and exit.");

		auto result = cli.parse({ServerInstance->Config->cmdline.argc, ServerInstance->Config->cmdline.argv});
		if (!result)
		{
			std::cerr << rang::style::bold << rang::fg::red << "Error: " << rang::style::reset << result.message() << '.' << std::endl;
			ServerInstance->Exit(EXIT_STATUS_ARGV);
		}

		if (do_help)
		{
			std::cout << cli << std::endl;
			ServerInstance->Exit(EXIT_STATUS_NOERROR);
		}

		if (do_version)
		{
			std::cout << INSPIRCD_VERSION << std::endl;
			ServerInstance->Exit(EXIT_STATUS_NOERROR);
		}

		// Store the relevant parsed arguments
		if (!config.empty())
			ServerInstance->ConfigFileName = ExpandPath(config.c_str());
		ServerInstance->Config->cmdline.forcedebug = do_debug;
		ServerInstance->Config->cmdline.nofork = do_nofork;
		ServerInstance->Config->cmdline.runasroot = do_runasroot;
		ServerInstance->Config->cmdline.writelog = !do_nolog;
		ServerInstance->Config->cmdline.writepid = !do_nopid;
	}
	// Seeds the random number generator if applicable.
	void SeedRng(timespec ts)
	{
#if defined _WIN32
		srand(ts.tv_nsec ^ ts.tv_sec);
#elif !defined HAS_ARC4RANDOM_BUF
		srandom(static_cast<int>(ts.tv_nsec ^ ts.tv_sec));
#endif
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
		signal(SIGTERM, InspIRCd::SetSignal);
	}

	void TryBindPorts()
	{
		FailedPortList pl;
		ServerInstance->BindPorts(pl);

		if (!pl.empty())
		{
			std::cout << rang::style::bold << rang::fg::red << "Warning!" << rang::style::reset << " Some of your listener" << (pl.size() == 1 ? "s" : "") << " failed to bind:" << std::endl
				<< std::endl;

			for (const auto& fp : pl)
			{
				std::cout << "  ";
				if (fp.sa.family() != AF_UNSPEC)
					std::cout << rang::style::bold << fp.sa.str() << rang::style::reset << ": ";

				std::cout << fp.error << '.' << std::endl
					<< "  " << "Created from <bind> tag at " << fp.tag->source.str() << std::endl
					<< std::endl;
			}

			std::cout << rang::style::bold << "Hints:" << rang::style::reset << std::endl
				<< "- For TCP/IP listeners try using a public IP address in <bind:address> instead" << std::endl
				<< "  of * or leaving it blank." << std::endl
				<< "- For UNIX socket listeners try enabling <bind:rewrite> to replace old sockets." << std::endl;
		}
	}

	// Required for returning the proper value of EXIT_SUCCESS for the parent process.
	void VoidSignalHandler(int)
	{
		InspIRCd::QuickExit(EXIT_STATUS_NOERROR);
	}
}

void InspIRCd::Cleanup()
{
	// Close all listening sockets
	for (auto* port : ports)
	{
		port->Cull();
		delete port;
	}
	ports.clear();

	// Tell modules that we're shutting down.
	const std::string quitmsg = "Server shutting down";
	FOREACH_MOD(OnShutdown, (quitmsg));

	// Disconnect all local users
	const UserManager::LocalList& list = Users.GetLocalUsers();
	while (!list.empty())
		ServerInstance->Users.QuitUser(list.front(), quitmsg);

	GlobalCulls.Apply();
	Modules.UnloadAll();

	/* Delete objects dynamically allocated in constructor (destructor would be more appropriate, but we're likely exiting) */
	/* Must be deleted before modes as it decrements modelines */
	if (FakeClient)
	{
		delete FakeClient->server;
		FakeClient->Cull();
	}
	stdalgo::delete_zero(this->FakeClient);
	stdalgo::delete_zero(this->XLines);
	stdalgo::delete_zero(this->Config);
	SocketEngine::Deinit();
	Logs.CloseLogs();
}

void InspIRCd::WritePID()
{
	if (!ServerInstance->Config->cmdline.writepid)
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
		std::cout << "Failed to write PID-file '" << pidfile << "', exiting." << std::endl;
		this->Logs.Error("STARTUP", "Failed to write PID-file '{}', exiting.", pidfile);
		Exit(EXIT_STATUS_PID);
	}
}

InspIRCd::InspIRCd(int argc, char** argv)
{
	ServerInstance = this;

	UpdateTime();
	this->startup_time = TIME.tv_sec;

	IncreaseCoreDumpSize();
	SeedRng(TIME);
	SocketEngine::Init();

	this->Config = new ServerConfig;
	dynamic_reference_base::reset_all();
	this->XLines = new XLineManager;

	this->Config->cmdline.argv = argv;
	this->Config->cmdline.argc = argc;
	ParseOptions();

	{
		ServiceProvider* provs[] =
		{
			&rfcevents.numeric, &rfcevents.join, &rfcevents.part, &rfcevents.kick, &rfcevents.quit, &rfcevents.nick,
			&rfcevents.mode, &rfcevents.topic, &rfcevents.privmsg, &rfcevents.invite, &rfcevents.ping, &rfcevents.pong,
			&rfcevents.error
		};
		Modules.AddServices(provs, sizeof(provs)/sizeof(provs[0]));
	}

	std::cout << rang::style::bold << rang::fg::green << "InspIRCd - Internet Relay Chat Daemon" << rang::style::reset << std::endl
		<< "See " << rang::style::bold << rang::fg::green << "/INFO" << rang::style::reset << " for contributors & authors" << std::endl
		<< std::endl;

	Logs.RegisterServices();
	if (Config->cmdline.forcedebug)
		Logs.EnableDebugMode();

	if (!FindConfigFile(ConfigFileName))
	{
		this->Logs.Error("STARTUP", "Unable to open config file {}", ConfigFileName);
		std::cout << "ERROR: Cannot open config file: " << ConfigFileName << std::endl << "Exiting..." << std::endl;
		Exit(EXIT_STATUS_CONFIG);
	}

	SetSignals();
	if (!Config->cmdline.runasroot)
		CheckRoot();
	if (!Config->cmdline.nofork)
		ForkIntoBackground();

	std::cout << "InspIRCd Process ID: " << rang::style::bold << rang::fg::green << getpid() << rang::style::reset << std::endl;

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
		std::cout << "ERROR: Cannot open log files: " << ex.GetReason() << std::endl << "Exiting..." << std::endl;
		Exit(EXIT_STATUS_LOG);
	}

	// If we don't have a SID, generate one based on the server name and the server description
	if (Config->ServerId.empty())
		Config->ServerId = UIDGenerator::GenerateSID(Config->ServerName, Config->ServerDesc);

	// Initialize the UID generator with our sid
	this->UIDGen.init(Config->ServerId);

	// Create the server user for this server
	this->FakeClient = new FakeUser(Config->ServerId, Config->ServerName, Config->ServerDesc);

	// This is needed as all new XLines are marked pending until ApplyLines() is called
	this->XLines->ApplyLines();

	std::cout << std::endl;

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
		std::cout << "ERROR: Cannot open log files: " << ex.GetReason() << std::endl << "Exiting..." << std::endl;
		Exit(EXIT_STATUS_LOG);
	}

	std::cout << "InspIRCd is now running as '" << Config->ServerName << "'[" << Config->ServerId << "] with " << SocketEngine::GetMaxFds() << " max open sockets" << std::endl;

#ifndef _WIN32
	if (!Config->cmdline.nofork)
	{
		if (kill(getppid(), SIGTERM) == -1)
		{
			std::cout << "Error killing parent process: " << strerror(errno) << std::endl;
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
	if ((!Config->cmdline.nofork) && (!Config->cmdline.forcedebug))
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
		Logs.Normal("STARTUP", "Keeping pseudo-tty open as we are running in the foreground.");
	}
#else
	/* Set win32 service as running, if we are running as a service */
	SetServiceRunning();

	// Handle forking
	if(!Config->cmdline.nofork)
	{
		FreeConsole();
	}

	QueryPerformanceFrequency(&stats.QPFrequency);
#endif

	WritePID();
	DropRoot();

	Logs.Normal("STARTUP", "Startup complete as '{}'[{}], {} max open sockets", Config->ServerName,
		Config->ServerId, SocketEngine::GetMaxFds());
}

void InspIRCd::UpdateTime()
{
#if defined HAS_CLOCK_GETTIME
	clock_gettime(CLOCK_REALTIME, &TIME);
#elif defined _WIN32
	SYSTEMTIME st;
	GetSystemTime(&st);

	TIME.tv_sec = time(nullptr);
	TIME.tv_nsec = st.wMilliseconds;
#else
	struct timeval tv;
	gettimeofday(&tv, nullptr);

	TIME.tv_sec = tv.tv_sec;
	TIME.tv_nsec = tv.tv_usec * 1000;
#endif
}

void InspIRCd::Run()
{
	UpdateTime();
	time_t OLDTIME = TIME.tv_sec;

	while (true)
	{
		/* Check if there is a config thread which has finished executing but has not yet been freed */
		if (this->ConfigThread && this->ConfigThread->IsDone())
		{
			/* Rehash has completed */
			this->Logs.Normal("CONFIG", "New configuration has been read, applying...");
			ConfigThread->Stop();
			stdalgo::delete_zero(ConfigThread);
		}

		UpdateTime();

		// Normally we want to limit the mainloop to processing data
		// once a second but this can cause problems with testing
		// software like irctest. Don't define this unless you know
		// what you are doing.
#ifndef INSPIRCD_UNLIMITED_MAINLOOP
		if (TIME.tv_sec != OLDTIME)
#endif
		{
			CollectStats();
			CheckTimeSkip(OLDTIME, TIME.tv_sec);

			OLDTIME = TIME.tv_sec;

			if ((TIME.tv_sec % 3600) == 0)
				FOREACH_MOD(OnGarbageCollect, ());

			Timers.TickTimers();
			Users.DoBackgroundUserStuff();

			if ((TIME.tv_sec % 5) == 0)
			{
				FOREACH_MOD(OnBackgroundTimer, (TIME.tv_sec));
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

		if (s_signal)
		{
			this->SignalHandler(s_signal);
			s_signal = 0;
		}
	}
}

sig_atomic_t InspIRCd::s_signal = 0;

void InspIRCd::SetSignal(int signal)
{
	s_signal = signal;
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
