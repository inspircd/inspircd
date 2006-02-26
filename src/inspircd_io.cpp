/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include "inspircd_config.h"
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <string>
#include <unistd.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include "inspircd.h"
#include "inspircd_io.h"
#include "inspstring.h"
#include "helperfuncs.h"
#include "userprocess.h"
#include "xline.h"

extern ServerConfig *Config;
extern InspIRCd* ServerInstance;
extern int openSockfd[MAXSOCKS];
extern time_t TIME;

extern int MODCOUNT;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;

ServerConfig::ServerConfig()
{
	this->ClearStack();
	*TempDir = *ServerName = *Network = *ServerDesc = *AdminName = '\0';
	*HideWhoisServer = *AdminEmail = *AdminNick = *diepass = *restartpass = '\0';
	*CustomVersion = *motd = *rules = *PrefixQuit = *DieValue = *DNSServer = '\0';
	*OperOnlyStats = *ModPath = *MyExecutable = *DisabledCommands = *PID = '\0';
	log_file = NULL;
	nofork = false;
	unlimitcore = false;
	AllowHalfop = true;
	HideSplits = false;
	dns_timeout = 5;
	MaxTargets = 20;
	NetBufferSize = 10240;
	SoftLimit = MAXCLIENTS;
	MaxConn = SOMAXCONN;
	MaxWhoResults = 100;
	debugging = 0;
	LogLevel = DEFAULT;
	DieDelay = 5;
}

void ServerConfig::ClearStack()
{
	include_stack.clear();
}

Module* ServerConfig::GetIOHook(int port)
{
	std::map<int,Module*>::iterator x = IOHookModule.find(port);
	return (x != IOHookModule.end() ? x->second : NULL);
}

bool ServerConfig::AddIOHook(int port, Module* iomod)
{
	if (!GetIOHook(port))
	{
		IOHookModule[port] = iomod;
		return true;
	}
	else
	{
		ModuleException err("Port already hooked by another module");
		throw(err);
		return false;
	}
}

bool ServerConfig::DelIOHook(int port)
{
	std::map<int,Module*>::iterator x = IOHookModule.find(port);
	if (x != IOHookModule.end())
	{
		IOHookModule.erase(x);
		return true;
	}
	return false;
}

bool ServerConfig::CheckOnce(char* tag, bool bail, userrec* user)
{
	int count = ConfValueEnum(tag,&Config->config_f);
	if (count > 1)
	{
		if (bail)
		{
			printf("There were errors in your configuration:\nYou have more than one <%s> tag, this is not permitted.\n",tag);
			Exit(0);
		}
		else
		{
			if (user)
			{
				WriteServ(user->fd,"There were errors in your configuration:");
				WriteServ(user->fd,"You have more than one <%s> tag, this is not permitted.\n",tag);
			}
			else
			{
				WriteOpers("There were errors in the configuration file:");
				WriteOpers("You have more than one <%s> tag, this is not permitted.\n",tag);
			}
		}
		return false;
	}
	if (count < 1)
	{
		if (bail)
		{
			printf("There were errors in your configuration:\nYou have not defined a <%s> tag, this is required.\n",tag);
			Exit(0);
		}
		else
		{
			if (user)
			{
				WriteServ(user->fd,"There were errors in your configuration:");
				WriteServ(user->fd,"You have not defined a <%s> tag, this is required.",tag);
			}
			else
			{
				WriteOpers("There were errors in the configuration file:");
				WriteOpers("You have not defined a <%s> tag, this is required.",tag);
			}
		}
		return false;
	}
	return true;
}

void ServerConfig::Read(bool bail, userrec* user)
{
	/** Yes yes, i know, this function is craq worthy of
	 * sirv. Its a mess, and some day i will tidy it.
	 * ...But that day will not be today. or probaby not
	 * tomorrow even, because it works fine.
	 */
        char dbg[MAXBUF],pauseval[MAXBUF],Value[MAXBUF],timeout[MAXBUF],NB[MAXBUF],flood[MAXBUF],MW[MAXBUF],MCON[MAXBUF],MT[MAXBUF];
        char AH[MAXBUF],AP[MAXBUF],AF[MAXBUF],DNT[MAXBUF],pfreq[MAXBUF],thold[MAXBUF],sqmax[MAXBUF],rqmax[MAXBUF],SLIMT[MAXBUF];
	char localmax[MAXBUF],globalmax[MAXBUF],HS[MAXBUF];
        ConnectClass c;
        std::stringstream errstr;
        include_stack.clear();

        if (!LoadConf(CONFIG_FILE,&Config->config_f,&errstr))
        {
                errstr.seekg(0);
                log(DEFAULT,"There were errors in your configuration:\n%s",errstr.str().c_str());
                if (bail)
                {
                        printf("There were errors in your configuration:\n%s",errstr.str().c_str());
                        Exit(0);
                }
                else
                {
                        char dataline[1024];
                        if (user)
                        {
                                WriteServ(user->fd,"NOTICE %s :There were errors in the configuration file:",user->nick);
                                while (!errstr.eof())
                                {
                                        errstr.getline(dataline,1024);
                                        WriteServ(user->fd,"NOTICE %s :%s",user->nick,dataline);
                                }
                        }
                        else
                        {
                                WriteOpers("There were errors in the configuration file:");
                                while (!errstr.eof())
                                {
                                        errstr.getline(dataline,1024);
                                        WriteOpers(dataline);
                                }
                        }
                        return;
                }
        }

	/* Check we dont have more than one of singular tags
	 */
	if (!CheckOnce("server",bail,user) || !CheckOnce("admin",bail,user) || !CheckOnce("files",bail,user)
		|| !CheckOnce("power",bail,user) || !CheckOnce("options",bail,user) || !CheckOnce("pid",bail,user))
	{
		return;
	}

        ConfValue("server","name",0,Config->ServerName,&Config->config_f);
        ConfValue("server","description",0,Config->ServerDesc,&Config->config_f);
        ConfValue("server","network",0,Config->Network,&Config->config_f);
        ConfValue("admin","name",0,Config->AdminName,&Config->config_f);
        ConfValue("admin","email",0,Config->AdminEmail,&Config->config_f);
        ConfValue("admin","nick",0,Config->AdminNick,&Config->config_f);
        ConfValue("files","motd",0,Config->motd,&Config->config_f);
        ConfValue("files","rules",0,Config->rules,&Config->config_f);
        ConfValue("power","diepass",0,Config->diepass,&Config->config_f);
        ConfValue("power","pause",0,pauseval,&Config->config_f);
        ConfValue("power","restartpass",0,Config->restartpass,&Config->config_f);
        ConfValue("options","prefixquit",0,Config->PrefixQuit,&Config->config_f);
        ConfValue("die","value",0,Config->DieValue,&Config->config_f);
        ConfValue("options","loglevel",0,dbg,&Config->config_f);
        ConfValue("options","netbuffersize",0,NB,&Config->config_f);
        ConfValue("options","maxwho",0,MW,&Config->config_f);
        ConfValue("options","allowhalfop",0,AH,&Config->config_f);
        ConfValue("options","allowprotect",0,AP,&Config->config_f);
        ConfValue("options","allowfounder",0,AF,&Config->config_f);
        ConfValue("dns","server",0,Config->DNSServer,&Config->config_f);
        ConfValue("dns","timeout",0,DNT,&Config->config_f);
        ConfValue("options","moduledir",0,Config->ModPath,&Config->config_f);
        ConfValue("disabled","commands",0,Config->DisabledCommands,&Config->config_f);
        ConfValue("options","somaxconn",0,MCON,&Config->config_f);
        ConfValue("options","softlimit",0,SLIMT,&Config->config_f);
	ConfValue("options","operonlystats",0,Config->OperOnlyStats,&Config->config_f);
	ConfValue("options","customversion",0,Config->CustomVersion,&Config->config_f);
	ConfValue("options","maxtargets",0,MT,&Config->config_f);
	ConfValue("options","hidesplits",0,HS,&Config->config_f);
	ConfValue("options","hidewhois",0,Config->HideWhoisServer,&Config->config_f);
	ConfValue("options","tempdir",0,Config->TempDir,&Config->config_f);

	strlower(Config->ServerName);

	if (!*Config->TempDir)
		strlcpy(Config->TempDir,"/tmp",1024);
	Config->HideSplits = ((*HS == 'y') || (*HS == 'Y') || (*HS == '1') || (*HS == 't') || (*HS == 'T'));
        Config->SoftLimit = atoi(SLIMT);
	if (*MT)
		Config->MaxTargets = atoi(MT);
	if ((Config->MaxTargets < 0) || (Config->MaxTargets > 31))
	{
		log(DEFAULT,"WARNING: <options:maxtargets> value is greater than 31 or less than 0, set to 20.");
		Config->MaxTargets = 20;
	}
        if ((Config->SoftLimit < 1) || (Config->SoftLimit > MAXCLIENTS))
        {
                log(DEFAULT,"WARNING: <options:softlimit> value is greater than %d or less than 0, set to %d.",MAXCLIENTS,MAXCLIENTS);
                Config->SoftLimit = MAXCLIENTS;
        }
        Config->MaxConn = atoi(MCON);
        if (Config->MaxConn > SOMAXCONN)
                log(DEFAULT,"WARNING: <options:somaxconn> value may be higher than the system-defined SOMAXCONN value!");
        Config->NetBufferSize = atoi(NB);
        Config->MaxWhoResults = atoi(MW);
        Config->dns_timeout = atoi(DNT);
	if (!strchr(Config->ServerName,'.'))
	{
		log(DEFAULT,"WARNING: <server:name> '%s' is not a fully-qualified domain name. Changed to '%s%c'",Config->ServerName,Config->ServerName,'.');
		strlcat(Config->ServerName,".",MAXBUF);
	}
        if (!Config->dns_timeout)
                Config->dns_timeout = 5;
        if (!Config->MaxConn)
                Config->MaxConn = SOMAXCONN;
        if (!*Config->DNSServer)
	{
		// attempt to look up their nameserver from /etc/resolv.conf
		log(DEFAULT,"WARNING: <dns:server> not defined, attempting to find working server in /etc/resolv.conf...");
		ifstream resolv("/etc/resolv.conf");
		std::string nameserver;
		bool found_server = false;
		if (resolv.is_open())
		{
			while (resolv >> nameserver)
			{
				if ((nameserver == "nameserver") && (!found_server))
				{
					resolv >> nameserver;
					strlcpy(Config->DNSServer,nameserver.c_str(),MAXBUF);
					found_server = true;
					log(DEFAULT,"<dns:server> set to '%s' as first resolver in /etc/resolv.conf.",nameserver.c_str());
				}
			}
			if (!found_server)
			{
				log(DEFAULT,"/etc/resolv.conf contains no viable nameserver entries! Defaulting to nameserver '127.0.0.1'!");
				strlcpy(Config->DNSServer,"127.0.0.1",MAXBUF);
			}
		}
		else
		{
			log(DEFAULT,"/etc/resolv.conf can't be opened! Defaulting to nameserver '127.0.0.1'!");
        	        strlcpy(Config->DNSServer,"127.0.0.1",MAXBUF);
		}
	}
        if (!*Config->ModPath)
                strlcpy(Config->ModPath,MOD_PATH,MAXBUF);
        Config->AllowHalfop = ((!strcasecmp(AH,"true")) || (!strcasecmp(AH,"1")) || (!strcasecmp(AH,"yes")));
        if ((!Config->NetBufferSize) || (Config->NetBufferSize > 65535) || (Config->NetBufferSize < 1024))
        {
                log(DEFAULT,"No NetBufferSize specified or size out of range, setting to default of 10240.");
                Config->NetBufferSize = 10240;
        }
        if ((!Config->MaxWhoResults) || (Config->MaxWhoResults > 65535) || (Config->MaxWhoResults < 1))
        {
                log(DEFAULT,"No MaxWhoResults specified or size out of range, setting to default of 128.");
                Config->MaxWhoResults = 128;
        }
        Config->LogLevel = DEFAULT;
        if (!strcmp(dbg,"debug"))
        {
                Config->LogLevel = DEBUG;
                Config->debugging = 1;
        }
        if (!strcmp(dbg,"verbose"))
                Config->LogLevel = VERBOSE;
        if (!strcmp(dbg,"default"))
                Config->LogLevel = DEFAULT;
        if (!strcmp(dbg,"sparse"))
                Config->LogLevel = SPARSE;
        if (!strcmp(dbg,"none"))
                Config->LogLevel = NONE;

        readfile(Config->MOTD,Config->motd);
        log(DEFAULT,"Reading message of the day...");
        readfile(Config->RULES,Config->rules);
        log(DEFAULT,"Reading connect classes...");
        Classes.clear();
        for (int i = 0; i < ConfValueEnum("connect",&Config->config_f); i++)
        {
                *Value = 0;
                ConfValue("connect","allow",i,Value,&Config->config_f);
                ConfValue("connect","timeout",i,timeout,&Config->config_f);
                ConfValue("connect","flood",i,flood,&Config->config_f);
                ConfValue("connect","pingfreq",i,pfreq,&Config->config_f);
                ConfValue("connect","threshold",i,thold,&Config->config_f);
                ConfValue("connect","sendq",i,sqmax,&Config->config_f);
                ConfValue("connect","recvq",i,rqmax,&Config->config_f);
		ConfValue("connect","localmax",i,localmax,&Config->config_f);
		ConfValue("connect","globalmax",i,globalmax,&Config->config_f);
                if (*Value)
                {
                        c.host = Value;
                        c.type = CC_ALLOW;
                        strlcpy(Value,"",MAXBUF);
                        ConfValue("connect","password",i,Value,&Config->config_f);
                        c.pass = Value;
                        c.registration_timeout = 90; // default is 2 minutes
                        c.pingtime = 120;
                        c.flood = atoi(flood);
                        c.threshold = 5;
                        c.sendqmax = 262144; // 256k
                        c.recvqmax = 4096;   // 4k
			c.maxlocal = 3;
			c.maxglobal = 3;
			if (atoi(localmax)>0)
			{
				c.maxlocal = atoi(localmax);
			}
			if (atoi(globalmax)>0)
			{
				c.maxglobal = atoi(globalmax);
			}
                        if (atoi(thold)>0)
                        {
                                c.threshold = atoi(thold);
                        }
			else
			{
				c.threshold = 1;
				c.flood = 999;
				log(DEFAULT,"Warning: Connect allow line '%s' has no flood/threshold settings. Setting this tag to 999 lines in 1 second.",c.host.c_str());
			}
                        if (atoi(sqmax)>0)
                        {
                                c.sendqmax = atoi(sqmax);
                        }
                        if (atoi(rqmax)>0)
                        {
                                c.recvqmax = atoi(rqmax);
                        }
                        if (atoi(timeout)>0)
                        {
                                c.registration_timeout = atoi(timeout);
                        }
                        if (atoi(pfreq)>0)
                        {
                                c.pingtime = atoi(pfreq);
                        }
                        Classes.push_back(c);
		}
                else
                {
                        ConfValue("connect","deny",i,Value,&Config->config_f);
                        c.host = Value;
                        c.type = CC_DENY;
                        Classes.push_back(c);
                        log(DEBUG,"Read connect class type DENY, host=%s",c.host.c_str());
                }

        }
        log(DEFAULT,"Reading K lines,Q lines and Z lines from config...");
        read_xline_defaults();
        log(DEFAULT,"Applying K lines, Q lines and Z lines...");
        apply_lines(APPLY_ALL);

        ConfValue("pid","file",0,Config->PID,&Config->config_f);
        // write once here, to try it out and make sure its ok
        WritePID(Config->PID);

        log(DEFAULT,"Done reading configuration file, InspIRCd is now starting.");
        if (!bail)
        {
                log(DEFAULT,"Adding and removing modules due to rehash...");

                std::vector<std::string> old_module_names, new_module_names, added_modules, removed_modules;

                // store the old module names
                for (std::vector<std::string>::iterator t = module_names.begin(); t != module_names.end(); t++)
                {
                        old_module_names.push_back(*t);
                }

                // get the new module names
                for (int count2 = 0; count2 < ConfValueEnum("module",&Config->config_f); count2++)
                {
                        ConfValue("module","name",count2,Value,&Config->config_f);
                        new_module_names.push_back(Value);
                }

                // now create a list of new modules that are due to be loaded
                // and a seperate list of modules which are due to be unloaded
                for (std::vector<std::string>::iterator _new = new_module_names.begin(); _new != new_module_names.end(); _new++)
                {
                        bool added = true;
                        for (std::vector<std::string>::iterator old = old_module_names.begin(); old != old_module_names.end(); old++)
                        {
                                if (*old == *_new)
                                        added = false;
                        }
                        if (added)
                                added_modules.push_back(*_new);
                }
                for (std::vector<std::string>::iterator oldm = old_module_names.begin(); oldm != old_module_names.end(); oldm++)
                {
                        bool removed = true;
                        for (std::vector<std::string>::iterator newm = new_module_names.begin(); newm != new_module_names.end(); newm++)
                        {
                                if (*newm == *oldm)
                                        removed = false;
                        }
                        if (removed)
                                removed_modules.push_back(*oldm);
                }
                // now we have added_modules, a vector of modules to be loaded, and removed_modules, a vector of modules
                // to be removed.
                int rem = 0, add = 0;
                if (!removed_modules.empty())
                for (std::vector<std::string>::iterator removing = removed_modules.begin(); removing != removed_modules.end(); removing++)
                {
                        if (ServerInstance->UnloadModule(removing->c_str()))
                        {
                                WriteOpers("*** REHASH UNLOADED MODULE: %s",removing->c_str());
                                if (user)
					WriteServ(user->fd,"973 %s %s :Module %s successfully unloaded.",user->nick, removing->c_str(), removing->c_str());
                                rem++;
                        }
                        else
                        {
				if (user)
                                	WriteServ(user->fd,"972 %s %s :Failed to unload module %s: %s",user->nick, removing->c_str(), removing->c_str(), ServerInstance->ModuleError());
                        }
                }
                if (!added_modules.empty())
                for (std::vector<std::string>::iterator adding = added_modules.begin(); adding != added_modules.end(); adding++)
                {
                        if (ServerInstance->LoadModule(adding->c_str()))
                        {
                                WriteOpers("*** REHASH LOADED MODULE: %s",adding->c_str());
				if (user)
                                	WriteServ(user->fd,"975 %s %s :Module %s successfully loaded.",user->nick, adding->c_str(), adding->c_str());
                                add++;
                        }
                        else
                        {
				if (user)
                                	WriteServ(user->fd,"974 %s %s :Failed to load module %s: %s",user->nick, adding->c_str(), adding->c_str(), ServerInstance->ModuleError());
                        }
                }
                log(DEFAULT,"Successfully unloaded %lu of %lu modules and loaded %lu of %lu modules.",(unsigned long)rem,(unsigned long)removed_modules.size(),
													(unsigned long)add,(unsigned long)added_modules.size());
	}
}


void Exit (int status)
{
	if (Config->log_file)
		fclose(Config->log_file);
	send_error("Server shutdown.");
	exit (status);
}

void Killed(int status)
{
	if (Config->log_file)
		fclose(Config->log_file);
	send_error("Server terminated.");
	exit(status);
}

char* CleanFilename(char* name)
{
	char* p = name + strlen(name);
	while ((p != name) && (*p != '/')) p--;
	return (p != name ? ++p : p);
}


void Rehash(int status)
{
	WriteOpers("Rehashing config file %s due to SIGHUP",CleanFilename(CONFIG_FILE));
	fclose(Config->log_file);
	OpenLog(NULL,0);
	Config->Read(false,NULL);
	FOREACH_MOD(I_OnRehash,OnRehash(""));
}



void Start (void)
{
	printf("\033[1;32mInspire Internet Relay Chat Server, compiled %s at %s\n",__DATE__,__TIME__);
	printf("(C) ChatSpike Development team.\033[0m\n\n");
	printf("Developers:\033[1;32m     Brain, FrostyCoolSlug\033[0m\n");
	printf("Documentation:\033[1;32m  FrostyCoolSlug, w00t\033[0m\n");
	printf("Testers:\033[1;32m        typobox43, piggles, Lord_Zathras, CC\033[0m\n");
	printf("Name concept:\033[1;32m   Lord_Zathras\033[0m\n\n");
}

void WritePID(std::string filename)
{
	ofstream outfile(filename.c_str());
	if (outfile.is_open())
	{
		outfile << getpid();
		outfile.close();
	}
	else
	{
		printf("Failed to write PID-file '%s', exiting.\n",filename.c_str());
		log(DEFAULT,"Failed to write PID-file '%s', exiting.",filename.c_str());
		Exit(0);
	}
}

void SetSignals()
{
	signal (SIGALRM, SIG_IGN);
	signal (SIGHUP, Rehash);
	signal (SIGPIPE, SIG_IGN);
	signal (SIGTERM, Exit);
	signal (SIGSEGV, Error);
}


int DaemonSeed (void)
{
	int childpid;
	if ((childpid = fork ()) < 0)
		return (ERROR);
	else if (childpid > 0)
		exit (0);
	setsid ();
	umask (007);
	printf("InspIRCd Process ID: \033[1;32m%lu\033[0m\n",(unsigned long)getpid());

	if (Config->unlimitcore)
	{
		rlimit rl;
		if (getrlimit(RLIMIT_CORE, &rl) == -1)
		{
			log(DEFAULT,"Failed to getrlimit()!");
			return(FALSE);
		}
		else
		{
			rl.rlim_cur = rl.rlim_max;
			if (setrlimit(RLIMIT_CORE, &rl) == -1)
				log(DEFAULT,"setrlimit() failed, cannot increase coredump size.");
		}
	}
  
	return (TRUE);
}


/* Make Sure Modules Are Avaliable!
 * (BugFix By Craig.. See? I do work! :p)
 * Modified by brain, requires const char*
 * to work with other API functions
 */

bool FileExists (const char* file)
{
	FILE *input;
	if ((input = fopen (file, "r")) == NULL)
	{
		return(false);
	}
	else
	{
		fclose (input);
		return(true);
	}
}

/* ConfProcess does the following things to a config line in the following order:
 *
 * Processes the line for syntax errors as shown below
 *      (1) Line void of quotes or equals (a malformed, illegal tag format)
 *      (2) Odd number of quotes on the line indicating a missing quote
 *      (3) number of equals signs not equal to number of quotes / 2 (missing an equals sign)
 *      (4) Spaces between the opening bracket (<) and the keyword
 *      (5) Spaces between a keyword and an equals sign
 *      (6) Spaces between an equals sign and a quote
 * Removes trailing spaces
 * Removes leading spaces
 * Converts tabs to spaces
 * Turns multiple spaces that are outside of quotes into single spaces
 */

std::string ServerConfig::ConfProcess(char* buffer, long linenumber, std::stringstream* errorstream, bool &error, std::string filename)
{
	long number_of_quotes = 0;
	long number_of_equals = 0;
	bool has_open_bracket = false;
	bool in_quotes = false;
	error = false;
	if (!buffer)
	{
		return "";
	}
	// firstly clean up the line by stripping spaces from the start and end and converting tabs to spaces
	for (char* d = buffer; *d; d++)
		if (*d == 9)
			*d = ' ';
	while (*buffer == ' ') buffer++;
	while ((buffer[strlen(buffer)-1] == ' ') && (*buffer)) buffer[strlen(buffer)-1] = '\0';

	// empty lines are syntactically valid, as are comments
	if (!(*buffer) || buffer[0] == '#')
		return "";

	for (unsigned int c = 0; c < strlen(buffer); c++)
	{
		// convert all spaces that are OUTSIDE quotes into hardspace (0xA0) as this will make them easier to
		// search and replace later :)
		if ((!in_quotes) && (buffer[c] == ' '))
			buffer[c] = '\xA0';
		if ((buffer[c] == '<') && (!in_quotes))
		{
			has_open_bracket = true;
			if (strlen(buffer) == 1)
			{
				*errorstream << "Tag without identifier at " << filename << ":" << linenumber << endl;
				error = true;
				return "";
			}
			else if ((tolower(buffer[c+1]) < 'a') || (tolower(buffer[c+1]) > 'z'))
			{
				*errorstream << "Invalid characters in identifier at " << filename << ":" << linenumber << endl;
				error = true;
				return "";
			}
		}
		if (buffer[c] == '"')
		{
			number_of_quotes++;
			in_quotes = (!in_quotes);
		}
		if ((buffer[c] == '=') && (!in_quotes))
		{
			number_of_equals++;
			if (strlen(buffer) == c)
			{
				*errorstream << "Variable without a value at " << filename << ":" << linenumber << endl;
				error = true;
				return "";
			}
			else if (buffer[c+1] != '"')
			{
				*errorstream << "Variable name not followed immediately by its value at " << filename << ":" << linenumber << endl;
				error = true;
				return "";
			}
			else if (!c)
			{
				*errorstream << "Value without a variable (line starts with '=') at " << filename << ":" << linenumber << endl;
				error = true;
				return "";
			}
			else if (buffer[c-1] == '\xA0')
			{
				*errorstream << "Variable name not followed immediately by its value at " << filename << ":" << linenumber << endl;
				error = true;
				return "";
			}
		}
	}
	// no quotes, and no equals. something freaky.
	if ((!number_of_quotes) || (!number_of_equals) && (strlen(buffer)>2) && (buffer[0]=='<'))
	{
		*errorstream << "Malformed tag at " << filename << ":" << linenumber << endl;
		error = true;
		return "";
	}
	// odd number of quotes. thats just wrong.
	if ((number_of_quotes % 2) != 0)
	{
		*errorstream << "Missing \" at " << filename << ":" << linenumber << endl;
		error = true;
		return "";
	}
	if (number_of_equals < (number_of_quotes/2))
	{
		*errorstream << "Missing '=' at " << filename << ":" << linenumber << endl;
	}
	if (number_of_equals > (number_of_quotes/2))
	{
		*errorstream << "Too many '=' at " << filename << ":" << linenumber << endl;
	}

	std::string parsedata = buffer;
	// turn multispace into single space
	while (parsedata.find("\xA0\xA0") != std::string::npos)
	{
		parsedata.erase(parsedata.find("\xA0\xA0"),1);
	}

	// turn our hardspace back into softspace
	for (unsigned int d = 0; d < parsedata.length(); d++)
	{
		if (parsedata[d] == '\xA0')
			parsedata[d] = ' ';
	}

	// and we're done, the line is fine!
	return parsedata;
}

int ServerConfig::fgets_safe(char* buffer, size_t maxsize, FILE* &file)
{
	char c_read = '\0';
	unsigned int bufptr = 0;
	while ((!feof(file)) && (c_read != '\n') && (c_read != '\r') && (bufptr < maxsize))
	{
		c_read = fgetc(file);
		if ((c_read != '\n') && (c_read != '\r'))
			buffer[bufptr++] = c_read;
	}
	buffer[bufptr] = '\0';
	return bufptr;
}

bool ServerConfig::LoadConf(const char* filename, std::stringstream *target, std::stringstream* errorstream)
{
	target->str("");
	errorstream->str("");
	long linenumber = 1;
	// first, check that the file exists before we try to do anything with it
	if (!FileExists(filename))
	{
		*errorstream << "File " << filename << " not found." << endl;
		return false;
	}
	// Fix the chmod of the file to restrict it to the current user and group
	chmod(filename,0600);
	for (unsigned int t = 0; t < include_stack.size(); t++)
	{
		if (std::string(filename) == include_stack[t])
		{
			*errorstream << "File " << filename << " is included recursively (looped inclusion)." << endl;
			return false;
		}
	}
	include_stack.push_back(filename);
	// now open it
	FILE* conf = fopen(filename,"r");
	char buffer[MAXBUF];
	if (conf)
	{
		while (!feof(conf))
		{
			if (fgets_safe(buffer, MAXBUF, conf))
			{
				if ((!feof(conf)) && (buffer) && (strlen(buffer)))
				{
					if ((buffer[0] != '#') && (buffer[0] != '\r')  && (buffer[0] != '\n'))
					{
						if (!strncmp(buffer,"<include file=\"",15))
						{
							char* buf = buffer;
							char confpath[10240],newconf[10240];
							// include file directive
							buf += 15;	// advance to filename
							for (unsigned int j = 0; j < strlen(buf); j++)
							{
								if (buf[j] == '\\')
									buf[j] = '/';
								if (buf[j] == '"')
								{
									buf[j] = '\0';
									break;
								}
							}
							log(DEBUG,"Opening included file '%s'",buf);
							if (*buf != '/')
							{
								strlcpy(confpath,CONFIG_FILE,10240);
								if (strstr(confpath,"/inspircd.conf"))
								{
									// leaves us with just the path
									*(strstr(confpath,"/inspircd.conf")) = '\0';
								}
								snprintf(newconf,10240,"%s/%s",confpath,buf);
							}
							else strlcpy(newconf,buf,10240);
							std::stringstream merge(stringstream::in | stringstream::out);
							// recursively call LoadConf and get the new data, use the same errorstream
							if (LoadConf(newconf, &merge, errorstream))
							{
								// append to the end of the file
								std::string newstuff = merge.str();
								*target << newstuff;
							}
							else
							{
								// the error propogates up to its parent recursively
								// causing the config reader to bail at the top level.
								fclose(conf);
								return false;
							}
						}
						else
						{
							bool error = false;
							std::string data = this->ConfProcess(buffer,linenumber++,errorstream,error,filename);
							if (error)
							{
								return false;
							}
							*target << data;
						}
					}
					else linenumber++;
				}
			}
		}
		fclose(conf);
	}
	target->seekg(0);
	return true;
}

/* Counts the number of tags of a certain type within the config file, e.g. to enumerate opers */

int ServerConfig::EnumConf(std::stringstream *config, const char* tag)
{
	int ptr = 0;
	char buffer[MAXBUF], c_tag[MAXBUF], c, lastc;
	int in_token, in_quotes, tptr, idx = 0;

	std::string x = config->str();
	const char* buf = x.c_str();
	long bptr = 0;
	long len = config->str().length();
	
	ptr = 0;
	in_token = 0;
	in_quotes = 0;
	lastc = '\0';
	while (bptr<len)
	{
		lastc = c;
		c = buf[bptr++];
		if ((c == '#') && (lastc == '\n'))
		{
			while ((c != '\n') && (bptr<len))
			{
				lastc = c;
				c = buf[bptr++];
			}
		}
		if ((c == '<') && (!in_quotes))
		{
			tptr = 0;
			in_token = 1;
			do {
				c = buf[bptr++];
				if (c != ' ')
				{
					c_tag[tptr++] = c;
					c_tag[tptr] = '\0';
				}
			} while (c != ' ');
		}
		if (c == '"')
		{
			in_quotes = (!in_quotes);
		}
		if ((c == '>') && (!in_quotes))
		{
			in_token = 0;
			if (!strcmp(c_tag,tag))
			{
				/* correct tag, but wrong index */
				idx++;
			}
			c_tag[0] = '\0';
			buffer[0] = '\0';
			ptr = 0;
			tptr = 0;
		}
		if (c != '>')
		{
			if ((in_token) && (c != '\n') && (c != '\r'))
			{
				buffer[ptr++] = c;
				buffer[ptr] = '\0';
			}
		}
	}
	return idx;
}

/* Counts the number of values within a certain tag */

int ServerConfig::EnumValues(std::stringstream *config, const char* tag, int index)
{
	int ptr = 0;
	char buffer[MAXBUF], c_tag[MAXBUF], c, lastc;
	int in_token, in_quotes, tptr, idx = 0;
	
	bool correct_tag = false;
	int num_items = 0;

	const char* buf = config->str().c_str();
	long bptr = 0;
	long len = strlen(buf);
	
	ptr = 0;
	in_token = 0;
	in_quotes = 0;
	lastc = '\0';
	while (bptr<len)
	{
		lastc = c;
		c = buf[bptr++];
		if ((c == '#') && (lastc == '\n'))
		{
			while ((c != '\n') && (bptr<len))
			{
				lastc = c;
				c = buf[bptr++];
			}
		}
		if ((c == '<') && (!in_quotes))
		{
			tptr = 0;
			in_token = 1;
			do {
				c = buf[bptr++];
				if (c != ' ')
				{
					c_tag[tptr++] = c;
					c_tag[tptr] = '\0';
					
					if ((!strcmp(c_tag,tag)) && (idx == index))
					{
						correct_tag = true;
					}
				}
			} while (c != ' ');
		}
		if (c == '"')
		{
			in_quotes = (!in_quotes);
		}
		
		if ( (correct_tag) && (!in_quotes) && ( (c == ' ') || (c == '\n') || (c == '\r') ) )
		{
			num_items++;
		}
		if ((c == '>') && (!in_quotes))
		{
			in_token = 0;
			if (correct_tag)
				correct_tag = false;
			if (!strcmp(c_tag,tag))
			{
				/* correct tag, but wrong index */
				idx++;
			}
			c_tag[0] = '\0';
			buffer[0] = '\0';
			ptr = 0;
			tptr = 0;
		}
		if (c != '>')
		{
			if ((in_token) && (c != '\n') && (c != '\r'))
			{
				buffer[ptr++] = c;
				buffer[ptr] = '\0';
			}
		}
	}
	return num_items+1;
}



int ServerConfig::ConfValueEnum(char* tag, std::stringstream* config)
{
	return EnumConf(config,tag);
}



/* Retrieves a value from the config file. If there is more than one value of the specified
 * key and section (e.g. for opers etc) then the index value specifies which to retreive, e.g.
 *
 * ConfValue("oper","name",2,result);
 */

int ServerConfig::ReadConf(std::stringstream *config, const char* tag, const char* var, int index, char *result)
{
	int ptr = 0;
	char buffer[65535], c_tag[MAXBUF], c, lastc;
	int in_token, in_quotes, tptr, idx = 0;
	char* key;

	std::string x = config->str();
	const char* buf = x.c_str();
	long bptr = 0;
	long len = config->str().length();
	
	ptr = 0;
	in_token = 0;
	in_quotes = 0;
	lastc = '\0';
	c_tag[0] = '\0';
	buffer[0] = '\0';
	while (bptr<len)
	{
		lastc = c;
		c = buf[bptr++];
		// FIX: Treat tabs as spaces
		if (c == 9)
			c = 32;
		if ((c == '<') && (!in_quotes))
		{
			tptr = 0;
			in_token = 1;
			do {
				c = buf[bptr++];
				if (c != ' ')
				{
					c_tag[tptr++] = c;
					c_tag[tptr] = '\0';
				}
			// FIX: Tab can follow a tagname as well as space.
			} while ((c != ' ') && (c != 9));
		}
		if (c == '"')
		{
			in_quotes = (!in_quotes);
		}
		if ((c == '>') && (!in_quotes))
		{
			in_token = 0;
			if (idx == index)
			{
				if (!strcmp(c_tag,tag))
				{
					if ((buffer) && (c_tag) && (var))
					{
						key = strstr(buffer,var);
						if (!key)
						{
							/* value not found in tag */
							*result = 0;
							return 0;
						}
						else
						{
							key+=strlen(var);
							while (*key !='"')
							{
								if (!*key)
								{
									/* missing quote */
									*result = 0;
									return 0;
								}
								key++;
							}
							key++;
							for (unsigned j = 0; j < strlen(key); j++)
							{
								if (key[j] == '"')
								{
									key[j] = '\0';
								}
							}
							strlcpy(result,key,MAXBUF);
							return 1;
						}
					}
				}
			}
			if (!strcmp(c_tag,tag))
			{
				/* correct tag, but wrong index */
				idx++;
			}
			c_tag[0] = '\0';
			buffer[0] = '\0';
			ptr = 0;
			tptr = 0;
		}
		if (c != '>')
		{
			if ((in_token) && (c != '\n') && (c != '\r'))
			{
				buffer[ptr++] = c;
				buffer[ptr] = '\0';
			}
		}
	}
	*result = 0; // value or its tag not found at all
	return 0;
}



int ServerConfig::ConfValue(char* tag, char* var, int index, char *result,std::stringstream *config)
{
	ReadConf(config, tag, var, index, result);
	return 0;
}



// This will bind a socket to a port. It works for UDP/TCP
int BindSocket (int sockfd, struct sockaddr_in client, struct sockaddr_in server, int port, char* addr)
{
	memset((char *)&server,0,sizeof(server));
	struct in_addr addy;
	inet_aton(addr,&addy);
	server.sin_family = AF_INET;
	if (!strcmp(addr,""))
	{
		server.sin_addr.s_addr = htonl(INADDR_ANY);
	}
	else
	{
		server.sin_addr = addy;
	}
	server.sin_port = htons(port);
	if (bind(sockfd,(struct sockaddr*)&server,sizeof(server))<0)
	{
		return(ERROR);
	}
	else
	{
		listen(sockfd, Config->MaxConn);
		return(TRUE);
	}
}


// Open a TCP Socket
int OpenTCPSocket (void)
{
	int sockfd;
	int on = 1;
	struct linger linger = { 0 };
  
	if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
		return (ERROR);
	else
	{
		setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));
		/* This is BSD compatible, setting l_onoff to 0 is *NOT* http://web.irc.org/mla/ircd-dev/msg02259.html */
		linger.l_onoff = 1;
		linger.l_linger = 1;
		setsockopt(sockfd, SOL_SOCKET, SO_LINGER, (const char*)&linger,sizeof(linger));
		return (sockfd);
	}
}

int BindPorts()
{
        char configToken[MAXBUF], Addr[MAXBUF], Type[MAXBUF];
	sockaddr_in client,server;
        int clientportcount = 0;
	int BoundPortCount = 0;
        for (int count = 0; count < Config->ConfValueEnum("bind",&Config->config_f); count++)
        {
                Config->ConfValue("bind","port",count,configToken,&Config->config_f);
                Config->ConfValue("bind","address",count,Addr,&Config->config_f);
                Config->ConfValue("bind","type",count,Type,&Config->config_f);
                if ((!*Type) || (!strcmp(Type,"clients")))
                {
                        // modules handle server bind types now,
                        // its not a typo in the strcmp.
                        Config->ports[clientportcount] = atoi(configToken);
                        strlcpy(Config->addrs[clientportcount],Addr,256);
                        clientportcount++;
                        log(DEBUG,"InspIRCd: startup: read binding %s:%s [%s] from config",Addr,configToken, Type);
                }
        }
        int PortCount = clientportcount;

        for (int count = 0; count < PortCount; count++)
        {
                if ((openSockfd[BoundPortCount] = OpenTCPSocket()) == ERROR)
                {
                        log(DEBUG,"InspIRCd: startup: bad fd %lu",(unsigned long)openSockfd[BoundPortCount]);
                        return(ERROR);
                }
                if (BindSocket(openSockfd[BoundPortCount],client,server,Config->ports[count],Config->addrs[count]) == ERROR)
                {
                        log(DEFAULT,"InspIRCd: startup: failed to bind port %lu",(unsigned long)Config->ports[count]);
                }
                else    /* well we at least bound to one socket so we'll continue */
                {
                        BoundPortCount++;
                }
        }

        /* if we didn't bind to anything then abort */
        if (!BoundPortCount)
        {
                log(DEFAULT,"InspIRCd: startup: no ports bound, bailing!");
                printf("\nERROR: Was not able to bind any of %lu ports! Please check your configuration.\n\n", (unsigned long)PortCount);
                return (ERROR);
        }

        return BoundPortCount;
}

