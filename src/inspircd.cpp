/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Val Lorentz <progval+git@progval.net>
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018 Chris Novakovic
 *   Copyright (C) 2013, 2017-2022 Sadie Powell <sadie@witchery.services>
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


#include "inspircd.h"
#include <signal.h>

#ifndef _WIN32
	#include <unistd.h>
	#include <sys/resource.h>
	#include <getopt.h>
	#include <pwd.h> // setuid
	#include <grp.h> // setgid
#else
	/** Manages formatting lines written to stderr on Windows. */
	WindowsStream StandardError(STD_ERROR_HANDLE);

	/** Manages formatting lines written to stdout on Windows. */
	WindowsStream StandardOutput(STD_OUTPUT_HANDLE);
#endif

#include <fstream>
#include <iostream>
#include "xline.h"
#include "exitcodes.h"

InspIRCd* ServerInstance = NULL;

/** Separate from the other casemap tables so that code *can* still exclusively rely on RFC casemapping
 * if it must.
 *
 * This is provided as a pointer so that modules can change it to their custom mapping tables,
 * e.g. for national character support.
 */
unsigned const char *national_case_insensitive_map = rfc_case_insensitive_map;


/* Moved from exitcodes.h -- due to duplicate symbols -- Burlex
 * XXX this is a bit ugly. -- w00t
 */
const char* ExitCodes[] =
{
		"No error",								// 0
		"DIE command",							// 1
		"Config file error",					// 2
		"Logfile error",						// 3
		"POSIX fork failed",					// 4
		"Bad commandline parameters",			// 5
		"Can't write PID file",					// 6
		"SocketEngine could not initialize",	// 7
		"Refusing to start up as root",			// 8
		"Couldn't load module on startup",		// 9
		"Received SIGTERM"						// 10
};

namespace
{
	void VoidSignalHandler(int);

	// Warns a user running as root that they probably shouldn't.
	void CheckRoot()
	{
#ifndef _WIN32
		if (getegid() != 0 && geteuid() != 0)
			return;

		std::cout << con_red << "Warning!" << con_reset << " You have started as root. Running as root is generally not required" << std::endl
			<< "and may allow an attacker to gain access to your system if they find a way to" << std::endl
			<< "exploit your IRC server." << std::endl
			<< std::endl;
		if (isatty(fileno(stdout)))
		{
			std::cout << "InspIRCd will start in 30 seconds. If you are sure that you need to run as root" << std::endl
				<< "then you can pass the " << con_bright << "--runasroot" << con_reset << " option to disable this wait." << std::endl;
			sleep(30);
		}
		else
		{
			std::cout << "If you are sure that you need to run as root then you can pass the " << con_bright << "--runasroot" << con_reset << std::endl
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
			ServerInstance->SNO->WriteToSnoMask('a', "\002Performance warning!\002 Server clock jumped forwards by %lu seconds!", timediff);

		else if (timediff < -ServerInstance->Config->TimeSkipWarn)
			ServerInstance->SNO->WriteToSnoMask('a', "\002Performance warning!\002 Server clock jumped backwards by %lu seconds!", labs(timediff));
	}

	// Drops to the unprivileged user/group specified in <security:runas{user,group}>.
	void DropRoot()
	{
#ifndef _WIN32
		ConfigTag* security = ServerInstance->Config->ConfValue("security");

		const std::string SetGroup = security->getString("runasgroup");
		if (!SetGroup.empty())
		{
			errno = 0;
			if (setgroups(0, NULL) == -1)
			{
				ServerInstance->Logs->Log("STARTUP", LOG_DEFAULT, "setgroups() failed (wtf?): %s", strerror(errno));
				InspIRCd::QuickExit(EXIT_STATUS_CONFIG);
			}

			struct group* g = getgrnam(SetGroup.c_str());
			if (!g)
			{
				ServerInstance->Logs->Log("STARTUP", LOG_DEFAULT, "getgrnam(%s) failed (wrong group?): %s", SetGroup.c_str(), strerror(errno));
				InspIRCd::QuickExit(EXIT_STATUS_CONFIG);
			}

			if (setgid(g->gr_gid) == -1)
			{
				ServerInstance->Logs->Log("STARTUP", LOG_DEFAULT, "setgid(%d) failed (wrong group?): %s", g->gr_gid, strerror(errno));
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
				ServerInstance->Logs->Log("STARTUP", LOG_DEFAULT, "getpwnam(%s) failed (wrong user?): %s", SetUser.c_str(), strerror(errno));
				InspIRCd::QuickExit(EXIT_STATUS_CONFIG);
			}

			if (setuid(u->pw_uid) == -1)
			{
				ServerInstance->Logs->Log("STARTUP", LOG_DEFAULT, "setuid(%d) failed (wrong user?): %s", u->pw_uid, strerror(errno));
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
		if (GetFullPathName(path, MAX_PATH, configPath, NULL) > 0)
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
		if (FileSystem::FileExists(path))
			return true;

#ifdef _WIN32
		// Windows hides file extensions by default so try appending .txt to the path
		// to help users who have that feature enabled and can't create .conf files.
		const std::string txtpath = path + ".txt";
		if (FileSystem::FileExists(txtpath))
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
			ServerInstance->Logs->Log("STARTUP", LOG_DEFAULT, "fork() failed: %s", strerror(errno));
			std::cout << con_red << "Error:" << con_reset << " unable to fork into background: " << strerror(errno);
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
			ServerInstance->Logs->Log("STARTUP", LOG_DEFAULT, "Unable to increase core dump size: getrlimit(RLIMIT_CORE) failed: %s", strerror(errno));
			return;
		}

		rl.rlim_cur = rl.rlim_max;
		if (setrlimit(RLIMIT_CORE, &rl) == -1)
			ServerInstance->Logs->Log("STARTUP", LOG_DEFAULT, "Unable to increase core dump size: setrlimit(RLIMIT_CORE) failed: %s", strerror(errno));
#endif
	}

	// Parses the command line options.
	void ParseOptions()
	{
		int do_debug = 0, do_nofork = 0,    do_nolog = 0;
		int do_nopid = 0, do_runasroot = 0, do_version = 0;
		struct option longopts[] =
		{
			{ "config",    required_argument, NULL,          'c' },
			{ "debug",     no_argument,       &do_debug,     1 },
			{ "nofork",    no_argument,       &do_nofork,    1 },
			{ "nolog",     no_argument,       &do_nolog,     1 },
			{ "nopid",     no_argument,       &do_nopid,     1 },
			{ "runasroot", no_argument,       &do_runasroot, 1 },
			{ "version",   no_argument,       &do_version,   1 },
			{ 0, 0, 0, 0 }
		};

		char** argv = ServerInstance->Config->cmdline.argv;
		int ret;
		while ((ret = getopt_long(ServerInstance->Config->cmdline.argc, argv, ":c:", longopts, NULL)) != -1)
		{
			switch (ret)
			{
				case 0:
					// A long option was specified.
					break;

				case 'c':
					// The -c option was specified.
					ServerInstance->ConfigFileName = ExpandPath(optarg);
					break;

				default:
					// An unknown option was specified.
					std::cout << con_red << "Error:" <<  con_reset << " unknown option '" << argv[optind-1] << "'." << std::endl
						<< con_bright << "Usage: " << con_reset << argv[0] << " [--config <file>] [--debug] [--nofork] [--nolog]" << std::endl
						<< std::string(strlen(argv[0]) + 8, ' ') << "[--nopid] [--runasroot] [--version]" << std::endl;
					ServerInstance->Exit(EXIT_STATUS_ARGV);
					break;
			}
		}

		if (do_version)
		{
			std::cout << INSPIRCD_VERSION << std::endl;
			ServerInstance->Exit(EXIT_STATUS_NOERROR);
		}

		// Store the relevant parsed arguments
		ServerInstance->Config->cmdline.forcedebug = !!do_debug;
		ServerInstance->Config->cmdline.nofork = !!do_nofork;
		ServerInstance->Config->cmdline.runasroot = !!do_runasroot;
		ServerInstance->Config->cmdline.writelog = !do_nolog;
		ServerInstance->Config->cmdline.writepid = !do_nopid;
	}
	// Seeds the random number generator if applicable.
	void SeedRng(timespec ts)
	{
#if defined _WIN32
		srand(ts.tv_nsec ^ ts.tv_sec);
#elif !defined HAS_ARC4RANDOM_BUF
		srandom(ts.tv_nsec ^ ts.tv_sec);
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
			std::cout << con_red << "Warning!" << con_reset << " Some of your listener" << (pl.size() == 1 ? "s" : "") << " failed to bind:" << std::endl
				<< std::endl;

			for (FailedPortList::const_iterator iter = pl.begin(); iter != pl.end(); ++iter)
			{
				const FailedPort& fp = *iter;
				std::cout << "  " << con_bright << fp.sa.str() << con_reset << ": " << strerror(fp.error) << '.' << std::endl
					<< "  " << "Created from <bind> tag at " << fp.tag->getTagLocation() << std::endl
					<< std::endl;
			}

			std::cout << con_bright << "Hints:" << con_reset << std::endl
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
	for (unsigned int i = 0; i < ports.size(); i++)
	{
		ports[i]->cull();
		delete ports[i];
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
	Modules->UnloadAll();

	/* Delete objects dynamically allocated in constructor (destructor would be more appropriate, but we're likely exiting) */
	/* Must be deleted before modes as it decrements modelines */
	if (FakeClient)
	{
		delete FakeClient->server;
		FakeClient->cull();
	}
	stdalgo::delete_zero(this->FakeClient);
	stdalgo::delete_zero(this->XLines);
	stdalgo::delete_zero(this->Config);
	SocketEngine::Deinit();
	Logs->CloseLogs();
}

void InspIRCd::WritePID(const std::string& filename, bool exitonfail)
{
#ifndef _WIN32
	if (!ServerInstance->Config->cmdline.writepid)
	{
		this->Logs->Log("STARTUP", LOG_DEFAULT, "--nopid specified on command line; PID file not written.");
		return;
	}

	std::string fname = ServerInstance->Config->Paths.PrependRuntime(filename.empty() ? "inspircd.pid" : filename);
	std::ofstream outfile(fname.c_str());
	if (outfile.is_open())
	{
		outfile << getpid();
		outfile.close();
	}
	else
	{
		if (exitonfail)
			std::cout << "Failed to write PID-file '" << fname << "', exiting." << std::endl;
		this->Logs->Log("STARTUP", LOG_DEFAULT, "Failed to write PID-file '%s'%s", fname.c_str(), (exitonfail ? ", exiting." : ""));
		if (exitonfail)
			Exit(EXIT_STATUS_PID);
	}
#endif
}

InspIRCd::InspIRCd(int argc, char** argv)
	: FakeClient(NULL)
	, ConfigFileName(INSPIRCD_CONFIG_PATH "/inspircd.conf")
	, ConfigThread(NULL)
	, Config(NULL)
	, XLines(NULL)
	, PI(&DefaultProtocolInterface)
	, GenRandom(&DefaultGenRandom)
	, IsChannel(&DefaultIsChannel)
	, IsNick(&DefaultIsNick)
	, IsIdent(&DefaultIsIdent)
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

	std::cout << con_green << "InspIRCd - Internet Relay Chat Daemon" << con_reset << std::endl
		<< "See " << con_green << "/INFO" << con_reset << " for contributors & authors" << std::endl
		<< std::endl;

	if (Config->cmdline.forcedebug)
	{
		FILE* newstdout = fdopen(dup(STDOUT_FILENO), "w");
		FileWriter* fw = new FileWriter(newstdout, 1);
		FileLogStream* fls = new FileLogStream(LOG_RAWIO, fw);
		Logs->AddLogTypes("*", fls, true);
	}

	if (!FindConfigFile(ConfigFileName))
	{
		this->Logs->Log("STARTUP", LOG_DEFAULT, "Unable to open config file %s", ConfigFileName.c_str());
		std::cout << "ERROR: Cannot open config file: " << ConfigFileName << std::endl << "Exiting..." << std::endl;
		Exit(EXIT_STATUS_CONFIG);
	}

	SetSignals();
	if (!Config->cmdline.runasroot)
		CheckRoot();
	if (!Config->cmdline.nofork)
		ForkIntoBackground();

	std::cout << "InspIRCd Process ID: " << con_green << getpid() << con_reset << std::endl;

	/* During startup we read the configuration now, not in
	 * a separate thread
	 */
	this->Config->Read();
	this->Config->Apply(NULL, "");
	Logs->OpenFileLogs();

	// If we don't have a SID, generate one based on the server name and the server description
	if (Config->sid.empty())
		Config->sid = UIDGenerator::GenerateSID(Config->ServerName, Config->ServerDesc);

	// Initialize the UID generator with our sid
	this->UIDGen.init(Config->sid);

	// Create the server user for this server
	this->FakeClient = new FakeUser(Config->sid, Config->ServerName, Config->ServerDesc);

	// This is needed as all new XLines are marked pending until ApplyLines() is called
	this->XLines->ApplyLines();

	std::cout << std::endl;

	TryBindPorts();

	this->Modules->LoadAll();

	// Build ISupport as ModuleManager::LoadAll() does not do it
	this->ISupport.Build();

	std::cout << "InspIRCd is now running as '" << Config->ServerName << "'[" << Config->GetSID() << "] with " << SocketEngine::GetMaxFds() << " max open sockets" << std::endl;

#ifndef _WIN32
	if (!Config->cmdline.nofork)
	{
		if (kill(getppid(), SIGTERM) == -1)
		{
			std::cout << "Error killing parent process: " << strerror(errno) << std::endl;
			Logs->Log("STARTUP", LOG_DEFAULT, "Error killing parent process: %s",strerror(errno));
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
			Logs->Log("STARTUP", LOG_DEFAULT, "Failed to dup /dev/null to stdin.");
		if (dup2(fd, STDOUT_FILENO) < 0)
			Logs->Log("STARTUP", LOG_DEFAULT, "Failed to dup /dev/null to stdout.");
		if (dup2(fd, STDERR_FILENO) < 0)
			Logs->Log("STARTUP", LOG_DEFAULT, "Failed to dup /dev/null to stderr.");
		close(fd);
	}
	else
	{
		Logs->Log("STARTUP", LOG_DEFAULT, "Keeping pseudo-tty open as we are running in the foreground.");
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

	WritePID(Config->PID);
	DropRoot();

	Logs->Log("STARTUP", LOG_DEFAULT, "Startup complete as '%s'[%s], %lu max open sockets", Config->ServerName.c_str(),
		Config->GetSID().c_str(), (unsigned long)SocketEngine::GetMaxFds());
}

void InspIRCd::UpdateTime()
{
#if defined HAS_CLOCK_GETTIME
	clock_gettime(CLOCK_REALTIME, &TIME);
#elif defined _WIN32
	SYSTEMTIME st;
	GetSystemTime(&st);

	TIME.tv_sec = time(NULL);
	TIME.tv_nsec = st.wMilliseconds;
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);

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
			this->Logs->Log("CONFIG", LOG_DEBUG, "Detected ConfigThread exiting, tidying up...");

			this->ConfigThread->Finish();

			ConfigThread->join();
			delete ConfigThread;
			ConfigThread = NULL;
		}

		UpdateTime();

		/* Run background module timers every few seconds
		 * (the docs say modules should not rely on accurate
		 * timing using this event, so we dont have to
		 * time this exactly).
		 */
		if (TIME.tv_sec != OLDTIME)
		{
			CollectStats();
			CheckTimeSkip(OLDTIME, TIME.tv_sec);

			OLDTIME = TIME.tv_sec;

			if ((TIME.tv_sec % 3600) == 0)
				FOREACH_MOD(OnGarbageCollect, ());

			Timers.TickTimers(TIME.tv_sec);
			Users->DoBackgroundUserStuff();

			if ((TIME.tv_sec % 5) == 0)
			{
				FOREACH_MOD(OnBackgroundTimer, (TIME.tv_sec));
				SNO->FlushSnotices();
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

/* On posix systems, the flow of the program starts right here, with
 * ENTRYPOINT being a #define that defines main(). On Windows, ENTRYPOINT
 * defines smain() and the real main() is in the service code under
 * win32service.cpp. This allows the service control manager to control
 * the process where we are running as a windows service.
 */
ENTRYPOINT
{
	new InspIRCd(argc, argv);
	ServerInstance->Run();
	delete ServerInstance;
	return 0;
}
