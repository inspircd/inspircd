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


/* $Core */
#include "inspircd.h"
#include "inspircd_version.h"
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
#include "bancache.h"
#include "socketengine.h"
#include "socket.h"
#include "command_parse.h"
#include "exitcodes.h"
#include "caller.h"
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
		"No error", /* 0 */
		"DIE command", /* 1 */
		"execv() failed", /* 2 */
		"Internal error", /* 3 */
		"Config file error", /* 4 */
		"Logfile error", /* 5 */
		"POSIX fork failed", /* 6 */
		"Bad commandline parameters", /* 7 */
		"No ports could be bound", /* 8 */
		"Can't write PID file", /* 9 */
		"SocketEngine could not initialize", /* 10 */
		"Refusing to start up as root", /* 11 */
		"Found a <die> tag!", /* 12 */
		"Couldn't load module on startup", /* 13 */
		"Could not create windows forked process", /* 14 */
		"Received SIGTERM", /* 15 */
		"Bad command handler loaded", /* 16 */
		"RegisterServiceCtrlHandler failed", /* 17 */
		"UpdateSCMStatus failed", /* 18 */
		"CreateEvent failed" /* 19 */
};

template<typename T> static void DeleteZero(T*&n)
{
	T* t = n;
	n = NULL;
	delete t;
}

void InspIRCd::Cleanup()
{
	for (unsigned int i = 0; i < ports.size(); i++)
	{
		/* This calls the constructor and closes the listening socket */
		ports[i]->cull();
		delete ports[i];
	}
	ports.clear();

	/* Close all client sockets, or the new process inherits them */
	LocalUserList::reverse_iterator i = Users->local_users.rbegin();
	while (i != this->Users->local_users.rend())
	{
		User* u = *i++;
		Users->QuitUser(u, "Server shutdown");
	}

	GlobalCulls.Apply();
	Modules->UnloadAll();

	/* Delete objects dynamically allocated in constructor (destructor would be more appropriate, but we're likely exiting) */
	/* Must be deleted before modes as it decrements modelines */
	if (FakeClient)
		FakeClient->cull();
	if (Res)
		Res->cull();
	DeleteZero(this->FakeClient);
	DeleteZero(this->Users);
	DeleteZero(this->Modes);
	DeleteZero(this->XLines);
	DeleteZero(this->Parser);
	DeleteZero(this->stats);
	DeleteZero(this->Modules);
	DeleteZero(this->BanCache);
	DeleteZero(this->SNO);
	DeleteZero(this->Config);
	DeleteZero(this->Res);
	DeleteZero(this->chanlist);
	DeleteZero(this->PI);
	DeleteZero(this->Threads);
	DeleteZero(this->Timers);
	DeleteZero(this->SE);
	/* Close logging */
	this->Logs->CloseLogs();
	DeleteZero(this->Logs);
}

void InspIRCd::Restart(const std::string &reason)
{
	/* SendError flushes each client's queue,
	 * regardless of writeability state
	 */
	this->SendError(reason);

	/* Figure out our filename (if theyve renamed it, we're boned) */
	std::string me;

	char** argv = Config->cmdline.argv;

#ifdef _WIN32
	char module[MAX_PATH];
	if (GetModuleFileNameA(NULL, module, MAX_PATH))
		me = module;
#else
	me = argv[0];
#endif

	this->Cleanup();

	if (execv(me.c_str(), argv) == -1)
	{
		/* Will raise a SIGABRT if not trapped */
		throw CoreException(std::string("Failed to execv()! error: ") + strerror(errno));
	}
}

void InspIRCd::ResetMaxBans()
{
	for (chan_hash::const_iterator i = chanlist->begin(); i != chanlist->end(); i++)
		i->second->ResetMaxBans();
}

/** Because hash_map doesn't free its buckets when we delete items, we occasionally
 * recreate the hash to free them up.
 * We do this by copying the entries from the old hash to a new hash, causing all
 * empty buckets to be weeded out of the hash.
 * Since this is quite expensive, it's not done very often.
 */
void InspIRCd::RehashUsersAndChans()
{
	user_hash* old_users = Users->clientlist;
	Users->clientlist = new user_hash;
	for (user_hash::const_iterator n = old_users->begin(); n != old_users->end(); n++)
		Users->clientlist->insert(*n);
	delete old_users;

	user_hash* old_uuid = Users->uuidlist;
	Users->uuidlist = new user_hash;
	for (user_hash::const_iterator n = old_uuid->begin(); n != old_uuid->end(); n++)
		Users->uuidlist->insert(*n);
	delete old_uuid;

	chan_hash* old_chans = chanlist;
	chanlist = new chan_hash;
	for (chan_hash::const_iterator n = old_chans->begin(); n != old_chans->end(); n++)
		chanlist->insert(*n);
	delete old_chans;

	// Reset the already_sent IDs so we don't wrap it around and drop a message
	LocalUser::already_sent_id = 0;
	for (LocalUserList::const_iterator i = Users->local_users.begin(); i != Users->local_users.end(); i++)
	{
		(**i).already_sent = 0;
		(**i).RemoveExpiredInvites();
	}

	// HACK: ELines are not expired properly at the moment but it can't be fixed as
	// the 2.0 XLine system is a spaghetti nightmare. Instead we skip over expired
	// ELines in XLineManager::CheckELines() and expire them here instead.
	ServerInstance->XLines->GetAll("E");
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

	int childpid;
	if ((childpid = fork ()) < 0)
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
		this->Logs->Log("STARTUP",DEFAULT,"Failed to getrlimit()!");
		return false;
	}
	rl.rlim_cur = rl.rlim_max;

	if (setrlimit(RLIMIT_CORE, &rl) == -1)
			this->Logs->Log("STARTUP",DEFAULT,"setrlimit() failed, cannot increase coredump size.");

	return true;
#endif
}

void InspIRCd::WritePID(const std::string& filename, bool exitonfail)
{
#ifndef _WIN32
	std::string fname(filename);
	if (fname.empty())
		fname = DATA_PATH "/inspircd.pid";
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
		this->Logs->Log("STARTUP",DEFAULT,"Failed to write PID-file '%s'%s",fname.c_str(), (exitonfail ? ", exiting." : ""));
		if (exitonfail)
			Exit(EXIT_STATUS_PID);
	}
#endif
}

InspIRCd::InspIRCd(int argc, char** argv) :
	 ConfigFileName(CONFIG_PATH "/inspircd.conf"),

	 /* Functor pointer initialisation.
	  *
	  * THIS MUST MATCH THE ORDER OF DECLARATION OF THE FUNCTORS, e.g. the methods
	  * themselves within the class.
	  */
	 NICKForced("NICKForced", NULL),
	 OperQuit("OperQuit", NULL),
	 GenRandom(&HandleGenRandom),
	 IsChannel(&HandleIsChannel),
	 IsSID(&HandleIsSID),
	 Rehash(&HandleRehash),
	 IsNick(&HandleIsNick),
	 IsIdent(&HandleIsIdent),
	 FloodQuitUser(&HandleFloodQuitUser),
	 OnCheckExemption(&HandleOnCheckExemption)
{
	ServerInstance = this;

	Extensions.Register(&NICKForced);
	Extensions.Register(&OperQuit);

	FailedPortList pl;
	int do_version = 0, do_nofork = 0, do_debug = 0,
	    do_nolog = 0, do_root = 0, do_testsuite = 0;    /* flag variables */
	int c = 0;

	// Initialize so that if we exit before proper initialization they're not deleted
	this->Logs = 0;
	this->Threads = 0;
	this->PI = 0;
	this->Users = 0;
	this->chanlist = 0;
	this->Config = 0;
	this->SNO = 0;
	this->BanCache = 0;
	this->Modules = 0;
	this->stats = 0;
	this->Timers = 0;
	this->Parser = 0;
	this->XLines = 0;
	this->Modes = 0;
	this->Res = 0;
	this->ConfigThread = NULL;
	this->FakeClient = NULL;

	UpdateTime();
	this->startup_time = TIME.tv_sec;

	// This must be created first, so other parts of Insp can use it while starting up
	this->Logs = new LogManager;

	SE = CreateSocketEngine();

	this->Threads = new ThreadEngine;

	/* Default implementation does nothing */
	this->PI = new ProtocolInterface;

	this->s_signal = 0;

	// Create base manager classes early, so nothing breaks
	this->Users = new UserManager;

	this->Users->clientlist = new user_hash();
	this->Users->uuidlist = new user_hash();
	this->chanlist = new chan_hash();

	this->Config = new ServerConfig;
	this->SNO = new SnomaskManager;
	this->BanCache = new BanCacheManager;
	this->Modules = new ModuleManager();
	this->stats = new serverstats();
	this->Timers = new TimerManager;
	this->Parser = new CommandParser;
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
		{ "logfile",	required_argument,	NULL,		'f'	},
		{ "config",	required_argument,	NULL,		'c'	},
		{ "debug",	no_argument,		&do_debug,	1	},
		{ "nolog",	no_argument,		&do_nolog,	1	},
		{ "runasroot",	no_argument,		&do_root,	1	},
		{ "version",	no_argument,		&do_version,	1	},
		{ "testsuite",	no_argument,		&do_testsuite,	1	},
		{ 0, 0, 0, 0 }
	};

	int index;
	while ((c = getopt_long(argc, argv, ":c:f:", longopts, &index)) != -1)
	{
		switch (c)
		{
			case 'f':
				/* Log filename was set */
				Config->cmdline.startup_log = optarg;
			break;
			case 'c':
				/* Config filename was set */
				ConfigFileName = optarg;
			break;
			case 0:
				/* getopt_long_only() set an int variable, just keep going */
			break;
			case '?':
				/* Unknown parameter */
			default:
				/* Fall through to handle other weird values too */
				std::cout << "Unknown parameter '" << argv[optind-1] << "'" << std::endl;
				std::cout << "Usage: " << argv[0] << " [--nofork] [--nolog] [--debug] [--logfile <filename>] " << std::endl <<
					std::string(static_cast<int>(8+strlen(argv[0])), ' ') << "[--runasroot] [--version] [--config <config>] [--testsuite]" << std::endl;
				Exit(EXIT_STATUS_ARGV);
			break;
		}
	}

	if (do_testsuite)
		do_nofork = do_debug = true;

	if (do_version)
	{
		std::cout << std::endl << VERSION << " r" << REVISION << std::endl;
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
	Config->cmdline.TestSuite = (do_testsuite != 0);

	if (do_debug)
	{
		FileWriter* fw = new FileWriter(stdout);
		FileLogStream* fls = new FileLogStream(RAWIO, fw);
		Logs->AddLogTypes("*", fls, true);
	}
	else if (!this->OpenLog(argv, argc))
	{
		std::cout << "ERROR: Could not open initial logfile " << Config->cmdline.startup_log << ": " << strerror(errno) << std::endl << std::endl;
		Exit(EXIT_STATUS_LOG);
	}

	if (!ServerConfig::FileExists(ConfigFileName.c_str()))
	{
#ifdef _WIN32
		/* Windows can (and defaults to) hide file extensions, so let's play a bit nice for windows users. */
		std::string txtconf = this->ConfigFileName;
		txtconf.append(".txt");

		if (ServerConfig::FileExists(txtconf.c_str()))
		{
			ConfigFileName = txtconf;
		}
		else
#endif
		{
			std::cout << "ERROR: Cannot open config file: " << ConfigFileName << std::endl << "Exiting..." << std::endl;
			this->Logs->Log("STARTUP",DEFAULT,"Unable to open config file %s", ConfigFileName.c_str());
			Exit(EXIT_STATUS_CONFIG);
		}
	}

	std::cout << con_green << "Inspire Internet Relay Chat Server" << con_reset << ", compiled on " __DATE__ " at " __TIME__ << std::endl;
	std::cout << con_green << "(C) InspIRCd Development Team." << con_reset << std::endl << std::endl;
	std::cout << "Developers:" << std::endl;
	std::cout << con_green << "\tBrain, FrostyCoolSlug, w00t, Om, Special, peavey" << std::endl;
	std::cout << "\taquanight, psychon, dz, danieldg, jackmcbarn" << std::endl;
	std::cout << "\tAttila" << con_reset << std::endl << std::endl;
	std::cout << "Others:\t\t\t" << con_green << "See /INFO Output" << con_reset << std::endl;

	this->Modes = new ModeParser;

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
			Logs->Log("STARTUP", DEFAULT, "ERROR: could not go into daemon mode. Shutting down.");
			Exit(EXIT_STATUS_FORK);
		}
	}

	SE->RecoverFromFork();

	/* During startup we don't actually initialize this
	 * in the thread engine.
	 */
	this->Config->Read();
	this->Config->Apply(NULL, "");
	Logs->OpenFileLogs();

	this->Res = new DNS();

	/*
	 * Initialise SID/UID.
 	 * For an explanation as to exactly how this works, and why it works this way, see GetUID().
	 *   -- w00t
 	 */
	if (Config->sid.empty())
	{
		// Generate one
		unsigned int sid = 0;
		char sidstr[4];

		for (const char* x = Config->ServerName.c_str(); *x; ++x)
			sid = 5 * sid + *x;
		for (const char* y = Config->ServerDesc.c_str(); *y; ++y)
			sid = 5 * sid + *y;
		sprintf(sidstr, "%03d", sid % 1000);

		Config->sid = sidstr;
	}

	/* set up fake client again this time with the correct uid */
	this->FakeClient = new FakeUser(Config->sid, Config->ServerName);

	// Get XLine to do it's thing.
	this->XLines->CheckELines();
	this->XLines->ApplyLines();

	int bounditems = BindPorts(pl);

	std::cout << std::endl;

	this->Modules->LoadAll();

	/* Just in case no modules were loaded - fix for bug #101 */
	this->BuildISupport();
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

	std::cout << "InspIRCd is now running as '" << Config->ServerName << "'[" << Config->GetSID() << "] with " << SE->GetMaxFds() << " max open sockets" << std::endl;

#ifndef _WIN32
	if (!Config->cmdline.nofork)
	{
		if (kill(getppid(), SIGTERM) == -1)
		{
			std::cout << "Error killing parent process: " << strerror(errno) << std::endl;
			Logs->Log("STARTUP", DEFAULT, "Error killing parent process: %s",strerror(errno));
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
	if ((!do_nofork) && (!do_testsuite) && (!Config->cmdline.forcedebug))
	{
		int fd = open("/dev/null", O_RDWR);

		fclose(stdin);
		fclose(stderr);
		fclose(stdout);

		if (dup2(fd, STDIN_FILENO) < 0)
			Logs->Log("STARTUP", DEFAULT, "Failed to dup /dev/null to stdin.");
		if (dup2(fd, STDOUT_FILENO) < 0)
			Logs->Log("STARTUP", DEFAULT, "Failed to dup /dev/null to stdout.");
		if (dup2(fd, STDERR_FILENO) < 0)
			Logs->Log("STARTUP", DEFAULT, "Failed to dup /dev/null to stderr.");
		close(fd);
	}
	else
	{
		Logs->Log("STARTUP", DEFAULT,"Keeping pseudo-tty open as we are running in the foreground.");
	}
#else
	/* Set win32 service as running, if we are running as a service */
	SetServiceRunning();

	// Handle forking
	if(!do_nofork)
	{
		FreeConsole();
	}

	QueryPerformanceFrequency(&stats->QPFrequency);
#endif

	Logs->Log("STARTUP", DEFAULT, "Startup complete as '%s'[%s], %d max open sockets", Config->ServerName.c_str(),Config->GetSID().c_str(), SE->GetMaxFds());

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
			this->Logs->Log("SETGROUPS", DEFAULT, "setgroups() failed (wtf?): %s", strerror(errno));
			this->QuickExit(0);
		}

		// setgid
		struct group *g;

		errno = 0;
		g = getgrnam(SetGroup.c_str());

		if (!g)
		{
			this->Logs->Log("SETGUID", DEFAULT, "getgrnam(%s) failed (wrong group?): %s", SetGroup.c_str(), strerror(errno));
			this->QuickExit(0);
		}

		ret = setgid(g->gr_gid);

		if (ret == -1)
		{
			this->Logs->Log("SETGUID", DEFAULT, "setgid() failed (wrong group?): %s", strerror(errno));
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
			this->Logs->Log("SETGUID", DEFAULT, "getpwnam(%s) failed (wrong user?): %s", SetUser.c_str(), strerror(errno));
			this->QuickExit(0);
		}

		int ret = setuid(u->pw_uid);

		if (ret == -1)
		{
			this->Logs->Log("SETGUID", DEFAULT, "setuid() failed (wrong user?): %s", strerror(errno));
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

int InspIRCd::Run()
{
	/* See if we're supposed to be running the test suite rather than entering the mainloop */
	if (Config->cmdline.TestSuite)
	{
		TestSuite* ts = new TestSuite;
		delete ts;
		Exit(0);
	}

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
			this->Logs->Log("CONFIG",DEBUG,"Detected ConfigThread exiting, tidying up...");

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
			stats->LastSampled = TIME;
			stats->LastCPU = ru.ru_utime;
#else
			if(QueryPerformanceCounter(&stats->LastSampled))
			{
				FILETIME CreationTime;
				FILETIME ExitTime;
				FILETIME KernelTime;
				FILETIME UserTime;
				GetProcessTimes(GetCurrentProcess(), &CreationTime, &ExitTime, &KernelTime, &UserTime);
				stats->LastCPU.dwHighDateTime = KernelTime.dwHighDateTime + UserTime.dwHighDateTime;
				stats->LastCPU.dwLowDateTime = KernelTime.dwLowDateTime + UserTime.dwLowDateTime;
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
			{
				this->RehashUsersAndChans();
				FOREACH_MOD(I_OnGarbageCollect, OnGarbageCollect());
			}

			Timers->TickTimers(TIME.tv_sec);
			this->DoBackgroundUserStuff();

			if ((TIME.tv_sec % 5) == 0)
			{
				FOREACH_MOD(I_OnBackgroundTimer,OnBackgroundTimer(TIME.tv_sec));
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
		this->SE->DispatchTrialWrites();
		this->SE->DispatchEvents();

		/* if any users were quit, take them out */
		GlobalCulls.Apply();
		AtomicActions.Run();

		if (s_signal)
		{
			this->SignalHandler(s_signal);
			s_signal = 0;
		}
	}

	return 0;
}

/**********************************************************************************/

/**
 * An ircd in five lines! bwahahaha. ahahahahaha. ahahah *cough*.
 */

/* this returns true when all modules are satisfied that the user should be allowed onto the irc server
 * (until this returns true, a user will block in the waiting state, waiting to connect up to the
 * registration timeout maximum seconds)
 */
bool InspIRCd::AllModulesReportReady(LocalUser* user)
{
	ModResult res;
	FIRST_MOD_RESULT(OnCheckReady, res, (user));
	return (res == MOD_RES_PASSTHRU);
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
