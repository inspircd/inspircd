/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "bancache.h"
#include <signal.h>

#ifndef WIN32
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
#endif

#include <fstream>
#include "xline.h"
#include "exitcodes.h"
#include "testsuite.h"

InspIRCd* ServerInstance = NULL;
int* mysig = NULL;

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
	std::vector<LocalUser*>::reverse_iterator i = Users->local_users.rbegin();
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

#ifdef WINDOWS
	char module[MAX_PATH];
	if (GetModuleFileName(NULL, module, MAX_PATH))
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
void InspIRCd::DoGarbageCollect()
{
	UpdateTime();
	ServerInstance->Logs->Log("core", DEBUG, "Garbage Collect started at %ld.%09ld", (long)Time(), Time_ns());
	IntModuleList& gclist = ServerInstance->Modules->EventHandlers[I_OnGarbageCollect];
	for(size_t i=0; i < gclist.size(); i++)
	{
		Module* mod = gclist[i];
		try
		{
			mod->OnGarbageCollect();
		}
		catch (CoreException& modexcept)
		{
			ServerInstance->Logs->Log("MODULE",DEFAULT,"Exception caught: %s",modexcept.GetReason());
		}
		UpdateTime();
		ServerInstance->Logs->Log("core", DEBUG, "Module %s GC finished at %ld.%09ld",
			mod->ModuleSourceFile.c_str(), (long)Time(), Time_ns());
	}
	user_hash* old_users = Users->clientlist;
	user_hash* old_uuid  = Users->uuidlist;
	chan_hash* old_chans = chanlist;

	Users->clientlist = new user_hash();
	Users->uuidlist = new user_hash();
	chanlist = new chan_hash();

	for (user_hash::const_iterator n = old_users->begin(); n != old_users->end(); n++)
		Users->clientlist->insert(*n);

	for (user_hash::const_iterator n = old_uuid->begin(); n != old_uuid->end(); n++)
		Users->uuidlist->insert(*n);

	for (chan_hash::const_iterator n = old_chans->begin(); n != old_chans->end(); n++)
		chanlist->insert(*n);

	delete old_users;
	delete old_uuid;
	delete old_chans;

	// Reset the already_sent IDs so we don't wrap it around and drop a message
	LocalUser::already_sent_id = 0;
	for (std::vector<LocalUser*>::const_iterator i = Users->local_users.begin(); i != Users->local_users.end(); i++)
	{
		(**i).already_sent = 0;
	}
	UpdateTime();
	ServerInstance->Logs->Log("core", DEBUG, "Garbage Collect finished at %ld.%09ld", (long)Time(), Time_ns());
}

void InspIRCd::SetSignals()
{
#ifndef WIN32
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
	exit(0);
}

bool InspIRCd::DaemonSeed()
{
#ifdef WINDOWS
	printf_c("InspIRCd Process ID: \033[1;32m%lu\033[0m\n", GetCurrentProcessId());
	return true;
#else
	signal(SIGTERM, InspIRCd::QuickExit);

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
	printf("InspIRCd Process ID: \033[1;32m%lu\033[0m\n",(unsigned long)getpid());

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

void InspIRCd::WritePID(const std::string &filename)
{
	std::string fname(filename);
	if (fname.empty())
		fname = "data/inspircd.pid";
	std::ofstream outfile(fname.c_str());
	if (outfile.is_open())
	{
		outfile << getpid();
		outfile.close();
	}
	else
	{
		printf("Failed to write PID-file '%s', exiting.\n",fname.c_str());
		this->Logs->Log("STARTUP",DEFAULT,"Failed to write PID-file '%s', exiting.",fname.c_str());
		Exit(EXIT_STATUS_PID);
	}
}

InspIRCd::InspIRCd(int argc, char** argv) :
	 ConfigFileName("conf/inspircd.conf"),

	 /* Functor pointer initialisation.
	  *
	  * THIS MUST MATCH THE ORDER OF DECLARATION OF THE FUNCTORS, e.g. the methods
	  * themselves within the class.
	  */
	 NICKForced(EXTENSIBLE_USER, "NICKForced", NULL),
	 RedirectJoin(EXTENSIBLE_USER, "RedirectJoin", NULL),
	 ForcedClass(EXTENSIBLE_USER, "ForcedClass", NULL),
	 OperQuit(EXTENSIBLE_USER, "OperQuit", NULL),
	 GenRandom(&HandleGenRandom),
	 IsChannel(&HandleIsChannel),
	 IsNick(&HandleIsNick),
	 IsIdent(&HandleIsIdent)
{
#ifdef WIN32
	// Strict, frequent checking of memory on debug builds
	_CrtSetDbgFlag ( _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );

	// Avoid erroneous frees on early exit
	WindowsIPC = 0;
#endif

	ServerInstance = this;

	Extensions.Register(&NICKForced);
	Extensions.Register(&RedirectJoin);
	Extensions.Register(&ForcedClass);
	Extensions.Register(&OperQuit);

	FailedPortList pl;
	int do_version = 0, do_nofork = 0, do_debug = 0,
	    do_nolog = 0, do_root = 0, do_testsuite = 0;    /* flag variables */
	int c = 0;

	// Initialize so that if we exit before proper initialization they're not deleted
	this->TraceData = 0;
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
	this->PendingRehash = 0;

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

	this->Users->unregistered_count = 0;

	this->Users->clientlist = new user_hash();
	this->Users->uuidlist = new user_hash();
	this->chanlist = new chan_hash();

	this->Config = new ServerConfig(REHASH_BOOT);
	this->SNO = new SnomaskManager;
	this->BanCache = new BanCacheManager;
	this->Modules = new ModuleManager();
	this->stats = new serverstats();
	this->Timers = new TimerManager;
	this->Parser = new CommandParser;
	this->XLines = new XLineManager;

	this->Config->cmdline.argv = argv;
	this->Config->cmdline.argc = argc;

	srandom(TIME.tv_nsec ^ TIME.tv_sec);

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
	while ((c = getopt_long(argc, argv, ":f:", longopts, &index)) != -1)
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
				printf("Unknown parameter '%s'\n", argv[optind-1]);
				printf("Usage: %s [--nofork] [--nolog] [--debug] [--logfile <filename>]\n%*s[--runasroot] [--version] [--config <config>] [--testsuite]\n", argv[0], static_cast<int>(8+strlen(argv[0])), " ");
				Exit(EXIT_STATUS_ARGV);
			break;
		}
	}

	if (do_testsuite)
		do_nofork = do_debug = true;

	if (do_version)
	{
		printf("\n%s r%s\n", VERSION, REVISION);
		Exit(EXIT_STATUS_NOERROR);
	}

#ifdef WIN32

	// Handle forking
	if(!do_nofork)
	{
		DWORD ExitCode = WindowsForkStart();
		if(ExitCode)
			exit(ExitCode);
	}

	// Set up winsock
	WSADATA wsadata;
	WSAStartup(MAKEWORD(2,0), &wsadata);
	ChangeWindowsSpecificPointers();
#endif

	/* Set the finished argument values */
	Config->cmdline.nofork = do_nofork;
	Config->cmdline.forcedebug = do_debug;
	Config->cmdline.writelog = !do_nolog;
	Config->cmdline.testsuite = do_testsuite;

	if (do_debug)
	{
		FileWriter* fw = new FileWriter(stdout);
		FileLogStream* fls = new FileLogStream(RAWIO, fw);
		Logs->AddLogTypes("*", fls, true);
	}
	else if (!this->OpenLog(argv, argc))
	{
		printf("ERROR: Could not open initial logfile %s: %s\n\n", Config->cmdline.startup_log.c_str(), strerror(errno));
		Exit(EXIT_STATUS_LOG);
	}

	if (!ServerConfig::FileExists(ConfigFileName.c_str()))
	{
#ifdef WIN32
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
			printf("ERROR: Cannot open config file: %s\nExiting...\n", ConfigFileName.c_str());
			this->Logs->Log("STARTUP",DEFAULT,"Unable to open config file %s", ConfigFileName.c_str());
			Exit(EXIT_STATUS_CONFIG);
		}
	}

	printf_c("\033[1;32mInspire Internet Relay Chat Server, compiled %s at %s\n",__DATE__,__TIME__);
	printf_c("(C) InspIRCd Development Team.\033[0m\n\n");
	printf_c("Developers:\n");
	printf_c("\t\033[1;32mBrain, FrostyCoolSlug, w00t, Om, Special\n");
	printf_c("\t\033[1;32mpeavey, aquanight, psychon, dz, danieldg\033[0m\n\n");
	printf_c("Others:\t\t\t\033[1;32mSee /INFO Output\033[0m\n");

	this->Modes = new ModeParser;

	if (!do_root)
		this->CheckRoot();
	else
	{
		printf("* WARNING * WARNING * WARNING * WARNING * WARNING * \n\n");
		printf("YOU ARE RUNNING INSPIRCD AS ROOT. THIS IS UNSUPPORTED\n");
		printf("AND IF YOU ARE HACKED, CRACKED, SPINDLED OR MUTILATED\n");
		printf("OR ANYTHING ELSE UNEXPECTED HAPPENS TO YOU OR YOUR\n");
		printf("SERVER, THEN IT IS YOUR OWN FAULT. IF YOU DID NOT MEAN\n");
		printf("TO START INSPIRCD AS ROOT, HIT CTRL+C NOW AND RESTART\n");
		printf("THE PROGRAM AS A NORMAL USER. YOU HAVE BEEN WARNED!\n");
		printf("\nInspIRCd starting in 20 seconds, ctrl+c to abort...\n");
		sleep(20);
	}

	this->SetSignals();

	if (!Config->cmdline.nofork)
	{
		if (!this->DaemonSeed())
		{
			printf("ERROR: could not go into daemon mode. Shutting down.\n");
			Logs->Log("STARTUP", DEFAULT, "ERROR: could not go into daemon mode. Shutting down.");
			Exit(EXIT_STATUS_FORK);
		}
	}

	SE->RecoverFromFork();

	/* During startup we don't actually initialize this
	 * in the thread engine.
	 */
	Config->Read();
	Config->Apply(NULL, "");
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

	printf("\n");

	this->Modules->LoadAll();

	/* Just in case no modules were loaded - fix for bug #101 */
	this->BuildISupport();
	Config->ApplyDisabled();

	if (!pl.empty())
	{
		printf("\nWARNING: Not all your client ports could be bound --\nstarting anyway with %d of %d client ports bound.\n\n",
			bounditems, bounditems + (int)pl.size());
		printf("The following port(s) failed to bind:\n");
		printf("Hint: Try using a public IP instead of blank or *\n\n");
		int j = 1;
		for (FailedPortList::iterator i = pl.begin(); i != pl.end(); i++, j++)
		{
			printf("%d.\tAddress: %s\tReason: %s\n", j, i->first.empty() ? "<all>" : i->first.c_str(), i->second.c_str());
		}
	}

	printf("\nInspIRCd is now running as '%s'[%s] with %d max open sockets\n",
		Config->ServerName.c_str(),Config->GetSID().c_str(), SE->GetMaxFds());

#ifndef WINDOWS
	if (!Config->cmdline.nofork)
	{
		if (kill(getppid(), SIGTERM) == -1)
		{
			printf("Error killing parent process: %s\n",strerror(errno));
			Logs->Log("STARTUP", DEFAULT, "Error killing parent process: %s",strerror(errno));
		}
	}

	if (isatty(0) && isatty(1) && isatty(2))
	{
		/* We didn't start from a TTY, we must have started from a background process -
		 * e.g. we are restarting, or being launched by cron. Dont kill parent, and dont
		 * close stdin/stdout
		 */
		if (!do_nofork)
		{
			fclose(stdin);
			fclose(stderr);
			if (!Config->cmdline.forcedebug)
				fclose(stdout);
		}
		else
		{
			Logs->Log("STARTUP", DEFAULT,"Keeping pseudo-tty open as we are running in the foreground.");
		}
	}
#else
	WindowsIPC = new IPC;
	if(!Config->nofork)
	{
		WindowsForkKillOwner();
		FreeConsole();
	}
	/* Set win32 service as running, if we are running as a service */
	SetServiceRunning();
#endif

	Logs->Log("STARTUP", DEFAULT, "Startup complete as '%s'[%s], %d max open sockets", Config->ServerName.c_str(),Config->GetSID().c_str(), SE->GetMaxFds());

#ifndef WIN32
	std::string SetUser = Config->GetTag("security")->getString("runasuser");
	std::string SetGroup = Config->GetTag("security")->getString("runasgroup");
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
			this->Logs->Log("SETGUID", DEFAULT, "getgrnam() failed (bad user?): %s", strerror(errno));
			this->QuickExit(0);
		}

		ret = setgid(g->gr_gid);

		if (ret == -1)
		{
			this->Logs->Log("SETGUID", DEFAULT, "setgid() failed (bad user?): %s", strerror(errno));
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
			this->Logs->Log("SETGUID", DEFAULT, "getpwnam() failed (bad user?): %s", strerror(errno));
			this->QuickExit(0);
		}

		int ret = setuid(u->pw_uid);

		if (ret == -1)
		{
			this->Logs->Log("SETGUID", DEFAULT, "setuid() failed (bad user?): %s", strerror(errno));
			this->QuickExit(0);
		}
	}
#endif

	this->WritePID(Config->PID);
}

void InspIRCd::UpdateTime()
{
#ifdef HAS_CLOCK_GETTIME
	clock_gettime(CLOCK_REALTIME, &TIME);
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	TIME.tv_sec = tv.tv_sec;
	TIME.tv_nsec = tv.tv_usec * 1000;
#endif
}

int InspIRCd::Run()
{
	/* See if we're supposed to be running the test suite rather than entering the mainloop */
	if (Config->cmdline.testsuite)
	{
		TestSuite* ts = new TestSuite;
		delete ts;
		Exit(0);
	}

	UpdateTime();
	time_t OLDTIME = TIME.tv_sec;

	while (true)
	{
#ifndef WIN32
		static rusage ru;
#else
		static time_t uptime;
#endif

		UpdateTime();
		Logs->Log("core", DEBUG, "Mainloop time step: %ld.%09ld", (long)Time(), Time_ns());

		/* Run background module timers every few seconds
		 * (the docs say modules shouldnt rely on accurate
		 * timing using this event, so we dont have to
		 * time this exactly).
		 */
		if (TIME.tv_sec != OLDTIME)
		{
#ifndef WIN32
			getrusage(RUSAGE_SELF, &ru);
			stats->LastSampled = TIME;
			stats->LastCPU = ru.ru_utime;
#else
			WindowsIPC->Check();
#endif

			/* Allow a buffer of two seconds drift on this so that ntpdate etc dont harass admins */
			if (TIME.tv_sec < OLDTIME - 2)
			{
				SNO->WriteToSnoMask('d', "\002EH?!\002 -- Time is flowing BACKWARDS in this dimension! Clock drifted backwards %lu secs.", (unsigned long)OLDTIME-TIME.tv_sec);
			}
			else if (TIME.tv_sec > OLDTIME + 2)
			{
				SNO->WriteToSnoMask('d', "\002EH?!\002 -- Time is jumping FORWARDS! Clock skipped %lu secs.", (unsigned long)TIME.tv_sec - OLDTIME);
			}
			OLDTIME = TIME.tv_sec;

			if ((TIME.tv_sec % 3600) == 0)
			{
				this->DoGarbageCollect();
			}

			Timers->TickTimers(TIME.tv_sec);
			this->DoBackgroundUserStuff();

			if ((TIME.tv_sec % 5) == 0)
			{
				FOREACH_MOD(I_OnBackgroundTimer,OnBackgroundTimer(TIME.tv_sec));
				SNO->FlushSnotices();
			}
			UpdateTime();
			Logs->Log("core", DEBUG, "Finished background events: %ld.%09ld", (long)Time(), Time_ns());
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

		UpdateTime();
		ServerInstance->Logs->Log("SOCKET", DEBUG, "Finished socket events: %ld.%09ld",
			(long)ServerInstance->Time(), ServerInstance->Time_ns());

		/* if any users were quit, take them out */
		GlobalCulls.Apply();
		AtomicActions.Run();

		if (this->s_signal)
		{
			this->SignalHandler(s_signal);
			this->s_signal = 0;
		}
	}

	return 0;
}

void InspIRCd::SetSignal(int signal)
{
	*mysig = signal;
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
	mysig = &ServerInstance->s_signal;
	ServerInstance->Run();
	delete ServerInstance;
	return 0;
}
