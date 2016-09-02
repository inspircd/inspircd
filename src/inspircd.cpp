/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2012 William Pitcock <nenolod@dereferenced.org>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2003-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2008 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2006-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006-2007 Oliver Lupton <oliverlupton@gmail.com>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Burlex <???@???>
 *   Copyright (C) 2003 Craig McLure <craig@chatspike.net>
 *   Copyright (C) 2003 randomdan <???@???>
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
	#include <dirent.h>
	#include <unistd.h>
	#include <sys/resource.h>
	#include <dlfcn.h>
	#include <getopt.h>

	/* Some systems don't define RUSAGE_SELF. This should fix them. */
	#ifndef RUSAGE_SELF
		#define RUSAGE_SELF 0
	#endif

	#include <pwd.h> // setuid
	#include <grp.h> // setgid
#else
	WORD g_wOriginalColors;
	WORD g_wBackgroundColor;
	HANDLE g_hStdout;
#endif

#include <fstream>
#include <iostream>
#include "xline.h"
#include "exitcodes.h"
#include "testsuite.h"

InspIRCd* ServerInstance = NULL;

/** Seperate from the other casemap tables so that code *can* still exclusively rely on RFC casemapping
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

#ifdef INSPIRCD_ENABLE_TESTSUITE
/** True if we have been told to run the testsuite from the commandline,
 * rather than entering the mainloop.
 */
static int do_testsuite = 0;
#endif

template<typename T> static void DeleteZero(T*&n)
{
	T* t = n;
	n = NULL;
	delete t;
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

	GlobalCulls.Apply();
	Modules->UnloadAll();

	/* Delete objects dynamically allocated in constructor (destructor would be more appropriate, but we're likely exiting) */
	/* Must be deleted before modes as it decrements modelines */
	if (FakeClient)
	{
		delete FakeClient->server;
		FakeClient->cull();
	}
	DeleteZero(this->FakeClient);
	DeleteZero(this->XLines);
	DeleteZero(this->Config);
	SocketEngine::Deinit();
	Logs->CloseLogs();
}

void InspIRCd::SetSignals()
{
#ifndef _WIN32
	signal(SIGALRM, SIG_IGN);
	signal(SIGHUP, InspIRCd::SetSignal);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
	/* We want E2BIG not a signal! */
	signal(SIGXFSZ, SIG_IGN);
#endif
	signal(SIGTERM, InspIRCd::SetSignal);
}

void InspIRCd::QuickExit(int status)
{
	exit(status);
}

// Required for returning the proper value of EXIT_SUCCESS for the parent process
static void VoidSignalHandler(int signalreceived)
{
	exit(0);
}

bool InspIRCd::DaemonSeed()
{
#ifdef _WIN32
	std::cout << "InspIRCd Process ID: " << con_green << GetCurrentProcessId() << con_reset << std::endl;
	return true;
#else
	// Do not use QuickExit here: It will exit with status SIGTERM which would break e.g. daemon scripts
	signal(SIGTERM, VoidSignalHandler);

	int childpid = fork();
	if (childpid < 0)
		return false;
	else if (childpid > 0)
	{
		/* We wait here for the child process to kill us,
		 * so that the shell prompt doesnt come back over
		 * the output.
		 * Sending a kill with a signal of 0 just checks
		 * if the child pid is still around. If theyre not,
		 * they threw an error and we should give up.
		 */
		while (kill(childpid, 0) != -1)
			sleep(1);
		exit(0);
	}
	setsid ();
	std::cout << "InspIRCd Process ID: " << con_green << getpid() << con_reset << std::endl;

	signal(SIGTERM, InspIRCd::SetSignal);

	rlimit rl;
	if (getrlimit(RLIMIT_CORE, &rl) == -1)
	{
		this->Logs->Log("STARTUP", LOG_DEFAULT, "Failed to getrlimit()!");
		return false;
	}
	rl.rlim_cur = rl.rlim_max;

	if (setrlimit(RLIMIT_CORE, &rl) == -1)
			this->Logs->Log("STARTUP", LOG_DEFAULT, "setrlimit() failed, cannot increase coredump size.");

	return true;
#endif
}

void InspIRCd::WritePID(const std::string& filename, bool exitonfail)
{
#ifndef _WIN32
	std::string fname(filename);
	if (fname.empty())
		fname = ServerInstance->Config->Paths.PrependData("inspircd.pid");
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

InspIRCd::InspIRCd(int argc, char** argv) :
	 ConfigFileName(INSPIRCD_CONFIG_PATH "/inspircd.conf"),
	 PI(&DefaultProtocolInterface),

	 /* Functor pointer initialisation.
	  *
	  * THIS MUST MATCH THE ORDER OF DECLARATION OF THE FUNCTORS, e.g. the methods
	  * themselves within the class.
	  */
	 OperQuit("operquit", ExtensionItem::EXT_USER, NULL),
	 GenRandom(&HandleGenRandom),
	 IsChannel(&HandleIsChannel),
	 IsNick(&HandleIsNick),
	 IsIdent(&HandleIsIdent),
	 OnCheckExemption(&HandleOnCheckExemption)
{
	ServerInstance = this;

	Extensions.Register(&OperQuit);

	FailedPortList pl;
	// Flag variables passed to getopt_long() later
	int do_version = 0, do_nofork = 0, do_debug = 0,
	    do_nolog = 0, do_root = 0;

	// Initialize so that if we exit before proper initialization they're not deleted
	this->Config = 0;
	this->XLines = 0;
	this->ConfigThread = NULL;
	this->FakeClient = NULL;

	UpdateTime();
	this->startup_time = TIME.tv_sec;

	SocketEngine::Init();

	this->Config = new ServerConfig;
	dynamic_reference_base::reset_all();
	this->XLines = new XLineManager;

	this->Config->cmdline.argv = argv;
	this->Config->cmdline.argc = argc;

#ifdef _WIN32
	srand(TIME.tv_nsec ^ TIME.tv_sec);

	// Initialize the console values
	g_hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO bufinf;
	if(GetConsoleScreenBufferInfo(g_hStdout, &bufinf))
	{
		g_wOriginalColors = bufinf.wAttributes & 0x00FF;
		g_wBackgroundColor = bufinf.wAttributes & 0x00F0;
	}
	else
	{
		g_wOriginalColors = FOREGROUND_RED|FOREGROUND_BLUE|FOREGROUND_GREEN;
		g_wBackgroundColor = 0;
	}
#else
	srandom(TIME.tv_nsec ^ TIME.tv_sec);
#endif

	struct option longopts[] =
	{
		{ "nofork",	no_argument,		&do_nofork,	1	},
		{ "config",	required_argument,	NULL,		'c'	},
		{ "debug",	no_argument,		&do_debug,	1	},
		{ "nolog",	no_argument,		&do_nolog,	1	},
		{ "runasroot",	no_argument,		&do_root,	1	},
		{ "version",	no_argument,		&do_version,	1	},
#ifdef INSPIRCD_ENABLE_TESTSUITE
		{ "testsuite",	no_argument,		&do_testsuite,	1	},
#endif
		{ 0, 0, 0, 0 }
	};

	int c;
	int index;
	while ((c = getopt_long(argc, argv, ":c:", longopts, &index)) != -1)
	{
		switch (c)
		{
			case 'c':
				/* Config filename was set */
				ConfigFileName = ServerInstance->Config->Paths.PrependConfig(optarg);
			break;
			case 0:
				/* getopt_long_only() set an int variable, just keep going */
			break;
			case '?':
				/* Unknown parameter */
			default:
				/* Fall through to handle other weird values too */
				std::cout << "Unknown parameter '" << argv[optind-1] << "'" << std::endl;
				std::cout << "Usage: " << argv[0] << " [--nofork] [--nolog] [--debug] [--config <config>]" << std::endl <<
					std::string(static_cast<int>(8+strlen(argv[0])), ' ') << "[--runasroot] [--version]" << std::endl;
				Exit(EXIT_STATUS_ARGV);
			break;
		}
	}

#ifdef INSPIRCD_ENABLE_TESTSUITE
	if (do_testsuite)
		do_nofork = do_debug = true;
#endif

	if (do_version)
	{
		std::cout << std::endl << INSPIRCD_VERSION << std::endl;
		Exit(EXIT_STATUS_NOERROR);
	}

#ifdef _WIN32
	// Set up winsock
	WSADATA wsadata;
	WSAStartup(MAKEWORD(2,2), &wsadata);
#endif

	/* Set the finished argument values */
	Config->cmdline.nofork = (do_nofork != 0);
	Config->cmdline.forcedebug = (do_debug != 0);
	Config->cmdline.writelog = !do_nolog;

	if (do_debug)
	{
		FileWriter* fw = new FileWriter(stdout);
		FileLogStream* fls = new FileLogStream(LOG_RAWIO, fw);
		Logs->AddLogTypes("*", fls, true);
	}

	if (!FileSystem::FileExists(ConfigFileName))
	{
#ifdef _WIN32
		/* Windows can (and defaults to) hide file extensions, so let's play a bit nice for windows users. */
		std::string txtconf = this->ConfigFileName;
		txtconf.append(".txt");

		if (FileSystem::FileExists(txtconf))
		{
			ConfigFileName = txtconf;
		}
		else
#endif
		{
			std::cout << "ERROR: Cannot open config file: " << ConfigFileName << std::endl << "Exiting..." << std::endl;
			this->Logs->Log("STARTUP", LOG_DEFAULT, "Unable to open config file %s", ConfigFileName.c_str());
			Exit(EXIT_STATUS_CONFIG);
		}
	}

	std::cout << con_green << "InspIRCd - Internet Relay Chat Daemon" << con_reset << ", compiled on " __DATE__ " at " __TIME__ << std::endl;
	std::cout << "For contributors & authors: " << con_green << "See /INFO Output" << con_reset << std::endl;

#ifndef _WIN32
	if (!do_root)
		this->CheckRoot();
	else
	{
		std::cout << "* WARNING * WARNING * WARNING * WARNING * WARNING *" << std::endl
		<< "YOU ARE RUNNING INSPIRCD AS ROOT. THIS IS UNSUPPORTED" << std::endl
		<< "AND IF YOU ARE HACKED, CRACKED, SPINDLED OR MUTILATED" << std::endl
		<< "OR ANYTHING ELSE UNEXPECTED HAPPENS TO YOU OR YOUR" << std::endl
		<< "SERVER, THEN IT IS YOUR OWN FAULT. IF YOU DID NOT MEAN" << std::endl
		<< "TO START INSPIRCD AS ROOT, HIT CTRL+C NOW AND RESTART" << std::endl
		<< "THE PROGRAM AS A NORMAL USER. YOU HAVE BEEN WARNED!" << std::endl << std::endl
		<< "InspIRCd starting in 20 seconds, ctrl+c to abort..." << std::endl;
		sleep(20);
	}
#endif

	this->SetSignals();

	if (!Config->cmdline.nofork)
	{
		if (!this->DaemonSeed())
		{
			std::cout << "ERROR: could not go into daemon mode. Shutting down." << std::endl;
			Logs->Log("STARTUP", LOG_DEFAULT, "ERROR: could not go into daemon mode. Shutting down.");
			Exit(EXIT_STATUS_FORK);
		}
	}

	SocketEngine::RecoverFromFork();

	/* During startup we read the configuration now, not in
	 * a seperate thread
	 */
	this->Config->Read();
	this->Config->Apply(NULL, "");
	Logs->OpenFileLogs();
	ModeParser::InitBuiltinModes();

	// If we don't have a SID, generate one based on the server name and the server description
	if (Config->sid.empty())
		Config->sid = UIDGenerator::GenerateSID(Config->ServerName, Config->ServerDesc);

	// Initialize the UID generator with our sid
	this->UIDGen.init(Config->sid);

	// Create the server user for this server
	this->FakeClient = new FakeUser(Config->sid, Config->ServerName, Config->ServerDesc);

	// This is needed as all new XLines are marked pending until ApplyLines() is called
	this->XLines->ApplyLines();

	int bounditems = BindPorts(pl);

	std::cout << std::endl;

	this->Modules->LoadAll();

	// Build ISupport as ModuleManager::LoadAll() does not do it
	this->ISupport.Build();
	Config->ApplyDisabledCommands(Config->DisabledCommands);

	if (!pl.empty())
	{
		std::cout << std::endl << "WARNING: Not all your client ports could be bound -- " << std::endl << "starting anyway with " << bounditems
			<< " of " << bounditems + (int)pl.size() << " client ports bound." << std::endl << std::endl;
		std::cout << "The following port(s) failed to bind:" << std::endl << std::endl;
		int j = 1;
		for (FailedPortList::iterator i = pl.begin(); i != pl.end(); i++, j++)
		{
			std::cout << j << ".\tAddress: " << (i->first.empty() ? "<all>" : i->first) << " \tReason: " << i->second << std::endl;
		}

		std::cout << std::endl << "Hint: Try using a public IP instead of blank or *" << std::endl;
	}

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
	if ((!do_nofork) && (!Config->cmdline.forcedebug))
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
	if(!do_nofork)
	{
		FreeConsole();
	}

	QueryPerformanceFrequency(&stats.QPFrequency);
#endif

	Logs->Log("STARTUP", LOG_DEFAULT, "Startup complete as '%s'[%s], %d max open sockets", Config->ServerName.c_str(),Config->GetSID().c_str(), SocketEngine::GetMaxFds());

#ifndef _WIN32
	std::string SetUser = Config->ConfValue("security")->getString("runasuser");
	std::string SetGroup = Config->ConfValue("security")->getString("runasgroup");
	if (!SetGroup.empty())
	{
		int ret;

		// setgroups
		ret = setgroups(0, NULL);

		if (ret == -1)
		{
			this->Logs->Log("STARTUP", LOG_DEFAULT, "setgroups() failed (wtf?): %s", strerror(errno));
			this->QuickExit(0);
		}

		// setgid
		struct group *g;

		errno = 0;
		g = getgrnam(SetGroup.c_str());

		if (!g)
		{
			this->Logs->Log("STARTUP", LOG_DEFAULT, "getgrnam(%s) failed (wrong group?): %s", SetGroup.c_str(), strerror(errno));
			this->QuickExit(0);
		}

		ret = setgid(g->gr_gid);

		if (ret == -1)
		{
			this->Logs->Log("STARTUP", LOG_DEFAULT, "setgid() failed (wrong group?): %s", strerror(errno));
			this->QuickExit(0);
		}
	}

	if (!SetUser.empty())
	{
		// setuid
		struct passwd *u;

		errno = 0;
		u = getpwnam(SetUser.c_str());

		if (!u)
		{
			this->Logs->Log("STARTUP", LOG_DEFAULT, "getpwnam(%s) failed (wrong user?): %s", SetUser.c_str(), strerror(errno));
			this->QuickExit(0);
		}

		int ret = setuid(u->pw_uid);

		if (ret == -1)
		{
			this->Logs->Log("STARTUP", LOG_DEFAULT, "setuid() failed (wrong user?): %s", strerror(errno));
			this->QuickExit(0);
		}
	}

	this->WritePID(Config->PID);
#endif
}

void InspIRCd::UpdateTime()
{
#ifdef _WIN32
	SYSTEMTIME st;
	GetSystemTime(&st);

	TIME.tv_sec = time(NULL);
	TIME.tv_nsec = st.wMilliseconds;
#else
	#ifdef HAS_CLOCK_GETTIME
		clock_gettime(CLOCK_REALTIME, &TIME);
	#else
		struct timeval tv;
		gettimeofday(&tv, NULL);
		TIME.tv_sec = tv.tv_sec;
		TIME.tv_nsec = tv.tv_usec * 1000;
	#endif
#endif
}

void InspIRCd::Run()
{
#ifdef INSPIRCD_ENABLE_TESTSUITE
	/* See if we're supposed to be running the test suite rather than entering the mainloop */
	if (do_testsuite)
	{
		TestSuite* ts = new TestSuite;
		delete ts;
		return;
	}
#endif

	UpdateTime();
	time_t OLDTIME = TIME.tv_sec;

	while (true)
	{
#ifndef _WIN32
		static rusage ru;
#endif

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
		 * (the docs say modules shouldnt rely on accurate
		 * timing using this event, so we dont have to
		 * time this exactly).
		 */
		if (TIME.tv_sec != OLDTIME)
		{
#ifndef _WIN32
			getrusage(RUSAGE_SELF, &ru);
			stats.LastSampled = TIME;
			stats.LastCPU = ru.ru_utime;
#else
			if(QueryPerformanceCounter(&stats.LastSampled))
			{
				FILETIME CreationTime;
				FILETIME ExitTime;
				FILETIME KernelTime;
				FILETIME UserTime;
				GetProcessTimes(GetCurrentProcess(), &CreationTime, &ExitTime, &KernelTime, &UserTime);
				stats.LastCPU.dwHighDateTime = KernelTime.dwHighDateTime + UserTime.dwHighDateTime;
				stats.LastCPU.dwLowDateTime = KernelTime.dwLowDateTime + UserTime.dwLowDateTime;
			}
#endif

			/* Allow a buffer of two seconds drift on this so that ntpdate etc dont harass admins */
			if (TIME.tv_sec < OLDTIME - 2)
			{
				SNO->WriteToSnoMask('d', "\002EH?!\002 -- Time is flowing BACKWARDS in this dimension! Clock drifted backwards %lu secs.", (unsigned long)(OLDTIME-TIME.tv_sec));
			}
			else if (TIME.tv_sec > OLDTIME + 2)
			{
				SNO->WriteToSnoMask('d', "\002EH?!\002 -- Time is jumping FORWARDS! Clock skipped %lu secs.", (unsigned long)(TIME.tv_sec - OLDTIME));
			}

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
