/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* Now with added unF! ;) */

using namespace std;

#include "inspircd_config.h"
#include "inspircd.h"
#include "inspircd_io.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <time.h>
#include <string>
#ifdef GCC3
#include <ext/hash_map>
#else
#include <hash_map>
#endif
#include <map>
#include <sstream>
#include <vector>
#include <deque>
#include <sched.h>
#ifdef THREADED_DNS
#include <pthread.h>
#endif
#include "users.h"
#include "ctables.h"
#include "globals.h"
#include "modules.h"
#include "dynamic.h"
#include "wildcard.h"
#include "message.h"
#include "mode.h"
#include "commands.h"
#include "xline.h"
#include "inspstring.h"
#include "dnsqueue.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "socketengine.h"
#include "userprocess.h"
#include "typedefs.h"
#include "command_parse.h"
#include "cull_list.h"

extern int MODCOUNT;
extern struct sockaddr_in client,server;
extern socklen_t length;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
extern std::vector<InspSocket*> module_sockets;
extern time_t TIME;
extern time_t OLDTIME;
extern std::vector<userrec*> local_users;
char LOG_FILE[MAXBUF];

extern InspIRCd* ServerInstance;
extern ServerConfig *Config;
extern userrec* fd_ref_table[MAX_DESCRIPTORS];
char data[65536];

extern user_hash clientlist;
extern chan_hash chanlist;

void ProcessUser(userrec* cu)
{
        int result = EAGAIN;
	if (cu->fd == FD_MAGIC_NUMBER)
		return;
        log(DEBUG,"Processing user with fd %d",cu->fd);
	if (Config->GetIOHook(cu->port))
	{
	        int result2 = 0;
		int MOD_RESULT = 0;
		try
		{
			MOD_RESULT = Config->GetIOHook(cu->port)->OnRawSocketRead(cu->fd,data,65535,result2);
	                log(DEBUG,"Data result returned by module: %d",MOD_RESULT);
		}
                catch (ModuleException& modexcept)
                {
                        log(DEBUG,"Module exception cought: %s",modexcept.GetReason()); \
                }
		if (MOD_RESULT < 0)
		{
			result = -EAGAIN;
		}
		else
		{
                	result = result2;
		}
	}
	else
	{
		result = cu->ReadData(data, 65535);
	}
        log(DEBUG,"Read result: %d",result);
        if ((result) && (result != -EAGAIN))
        {
                ServerInstance->stats->statsRecv += result;
                // perform a check on the raw buffer as an array (not a string!) to remove
                // characters 0 and 7 which are illegal in the RFC - replace them with spaces.
                // hopefully this should stop even more people whining about "Unknown command: *"
                for (int checker = 0; checker < result; checker++)
                {
                        if ((data[checker] == 0) || (data[checker] == 7))
                                data[checker] = ' ';
                }
                if (result > 0)
                        data[result] = '\0';
                userrec* current = cu;
                int currfd = current->fd;
                int floodlines = 0;
                // add the data to the users buffer
                if (result > 0)
                {
                        if (!current->AddBuffer(data))
                        {
                                // AddBuffer returned false, theres too much data in the user's buffer and theyre up to no good.
                                if (current->registered == 7)
				{
					// Make sure they arn't flooding long lines.
				        if (TIME > current->reset_due)
	                                {
        	                                current->reset_due = TIME + current->threshold;
                	                        current->lines_in = 0;
                        	        }
	                                current->lines_in++;
        	                        if (current->lines_in > current->flood)
                	                {
                        	                log(DEFAULT,"Excess flood from: %s!%s@%s",current->nick,current->ident,current->host);
                                	        WriteOpers("*** Excess flood from: %s!%s@%s",current->nick,current->ident,current->host);
                                        	kill_link(current,"Excess flood");
        	                                return;
                	                } else {
                                        	WriteServ(currfd, "NOTICE %s :Your previous line was too long and was not delivered (Over 512chars) Please shorten it.", current->nick);
						current->recvq = "";
					}
                                }
                                else
                                {
                                        WriteOpers("*** Excess flood from %s",(char*)inet_ntoa(current->ip4));
                                        log(DEFAULT,"Excess flood from: %s",(char*)inet_ntoa(current->ip4));
                                        add_zline(120,Config->ServerName,"Flood from unregistered connection",(char*)inet_ntoa(current->ip4));
                                        apply_lines(APPLY_ZLINES);
                                }
                                return;
                        }
                        if (current->recvq.length() > (unsigned)Config->NetBufferSize)
                        {
                                if (current->registered == 7)
                                {
                                        kill_link(current,"RecvQ exceeded");
                                }
                                else
                                {
                                        WriteOpers("*** Excess flood from %s",(char*)inet_ntoa(current->ip4));
                                        log(DEFAULT,"Excess flood from: %s",(char*)inet_ntoa(current->ip4));
                                        add_zline(120,Config->ServerName,"Flood from unregistered connection",(char*)inet_ntoa(current->ip4));
                                        apply_lines(APPLY_ZLINES);
                                }
                                return;
                        }
                        // while there are complete lines to process...
                        while (current->BufferIsReady())
                        {
                                floodlines++;
                                if (TIME > current->reset_due)
                                {
                                        current->reset_due = TIME + current->threshold;
                                        current->lines_in = 0;
                                }
                                current->lines_in++;
                                if (current->lines_in > current->flood)
                                {
                                        log(DEFAULT,"Excess flood from: %s!%s@%s",current->nick,current->ident,current->host);
                                        WriteOpers("*** Excess flood from: %s!%s@%s",current->nick,current->ident,current->host);
                                        kill_link(current,"Excess flood");
                                        return;
                                }
                                if ((floodlines > current->flood) && (current->flood != 0))
                                {
                                        if (current->registered == 7)
                                        {
                                                log(DEFAULT,"Excess flood from: %s!%s@%s",current->nick,current->ident,current->host);
                                                WriteOpers("*** Excess flood from: %s!%s@%s",current->nick,current->ident,current->host);
                                                kill_link(current,"Excess flood");
                                        }
                                        else
                                        {
                                                add_zline(120,Config->ServerName,"Flood from unregistered connection",(char*)inet_ntoa(current->ip4));
                                                apply_lines(APPLY_ZLINES);
                                        }
                                        return;
                                }
                                char sanitized[MAXBUF];
                                // use GetBuffer to copy single lines into the sanitized string
                                std::string single_line = current->GetBuffer();
                                current->bytes_in += single_line.length();
                                current->cmds_in++;
                                if (single_line.length()>512)
                                {
                                        log(DEFAULT,"Excess flood from: %s!%s@%s",current->nick,current->ident,current->host);
                                        WriteOpers("*** Excess flood from: %s!%s@%s",current->nick,current->ident,current->host);
                                        kill_link(current,"Excess flood");
                                        return;
                                }
                                strlcpy(sanitized,single_line.c_str(),MAXBUF);
                                if (*sanitized)
                                {
                                        userrec* old_comp = fd_ref_table[currfd];
                                        // we're gonna re-scan to check if the nick is gone, after every
                                        // command - if it has, we're gonna bail
                                        ServerInstance->Parser->ProcessBuffer(sanitized,current);
                                        // look for the user's record in case it's changed... if theyve quit,
                                        // we cant do anything more with their buffer, so bail.
                                        // there used to be an ugly, slow loop here. Now we have a reference
                                        // table, life is much easier (and FASTER)
                                        userrec* new_comp = fd_ref_table[currfd];
                                        if ((currfd < 0) || (!fd_ref_table[currfd]) || (old_comp != new_comp))
                                        {
                                                return;
                                        }
                                        else
                                        {
                                                /* The user is still here, flush their buffer */
                                                current->FlushWriteBuf();
                                        }
                                }
                        }
                        return;
                }
                if ((result == -1) && (errno != EAGAIN) && (errno != EINTR))
                {
                        log(DEBUG,"killing: %s",cu->nick);
                        kill_link(cu,strerror(errno));
                        return;
                }
        }
        // result EAGAIN means nothing read
        else if ((result == EAGAIN) || (result == -EAGAIN))
        {
        }
        else if (result == 0)
        {
                log(DEBUG,"InspIRCd: Exited: %s",cu->nick);
                kill_link(cu,"Client exited");
                log(DEBUG,"Bailing from client exit");
                return;
        }
}

void DoSocketTimeouts(time_t TIME)
{
        unsigned int numsockets = module_sockets.size();
        SocketEngine* SE = ServerInstance->SE;
        for (std::vector<InspSocket*>::iterator a = module_sockets.begin(); a < module_sockets.end(); a++)
        {
                InspSocket* s = (InspSocket*)*a;
                if (s->Timeout(TIME))
                {
                        log(DEBUG,"Socket poll returned false, close and bail");
                        SE->DelFd(s->GetFd());
                        s->Close();
                        module_sockets.erase(a);
                        delete s;
                        break;
                }
                if (module_sockets.size() != numsockets) break;
        }
}

/**
 * This function is called once a second from the mainloop.
 * It is intended to do background checking on all the user structs, e.g.
 * stuff like ping checks, registration timeouts, etc. This function is
 * also responsible for checking if InspSocket derived classes are timed out.
 */
void DoBackgroundUserStuff(time_t TIME)
{
        CullList* GlobalGoners = new CullList();
        for (std::vector<userrec*>::iterator count2 = local_users.begin(); count2 != local_users.end(); count2++)
        {
                /* Sanity checks for corrupted iterators (yes, really) */
                userrec* curr = NULL;
                if (*count2)
                        curr = (userrec*)(*count2);
                if ((long)curr == -1)
                        return;

                if (curr)
                {
                        // registration timeout -- didnt send USER/NICK/HOST in the time specified in
                        // their connection class.
                        if (((unsigned)TIME > (unsigned)curr->timeout) && (curr->registered != 7))
                        {
                                log(DEBUG,"InspIRCd: registration timeout: %s",curr->nick);
                                GlobalGoners->AddItem(curr,"Registration timeout");
                                continue;
                        }
			// user has signed on with USER/NICK/PASS, and dns has completed, all the modules
			// say this user is ok to proceed, fully connect them.
                        if ((TIME > curr->signon) && (curr->registered == 3) && (AllModulesReportReady(curr)))
                        {
                                curr->dns_done = true;
                                ServerInstance->stats->statsDnsBad++;
                                FullConnectUser(curr,GlobalGoners);
                                continue;
                        }
                        if ((curr->dns_done) && (curr->registered == 3) && (AllModulesReportReady(curr)))
                        {
                                log(DEBUG,"dns done, registered=3, and modules ready, OK");
                                FullConnectUser(curr,GlobalGoners);
                                continue;
                        }
			// It's time to PING this user. Send them a ping.
                        if ((TIME > curr->nping) && (curr->registered == 7))
                        {
				// This user didn't answer the last ping, remove them
				if (!curr->lastping)
				{
					GlobalGoners->AddItem(curr,"Ping timeout");
					continue;
				}
				Write(curr->fd,"PING :%s",Config->ServerName);
				curr->lastping = 0;
				curr->nping = TIME+curr->pingmax;
			}
			// We can flush the write buffer as the last thing we do, because if they
			// match any of the above conditions its no use flushing their buffer anyway.
			curr->FlushWriteBuf();
			if (curr->GetWriteError() != "")
			{
				GlobalGoners->AddItem(curr,curr->GetWriteError());
				continue;
			}
                }
        }
	/** Remove all the queued users who are due to be quit
	 */
	GlobalGoners->Apply();
	/** Free to memory used
	 */
	delete GlobalGoners;
        return;
}

void OpenLog(char** argv, int argc)
{
	if (!*LOG_FILE)
	{
		if (Config->logpath == "")
		{
		        Config->logpath = GetFullProgDir(argv,argc) + "/ircd.log";
		}
	}
	else
	{
		Config->log_file = fopen(LOG_FILE,"a+");
		if (!Config->log_file)
		{
			printf("ERROR: Could not write to logfile %s, bailing!\n\n",Config->logpath.c_str());
			Exit(ERROR);
		}
	}
        Config->log_file = fopen(Config->logpath.c_str(),"a+");
        if (!Config->log_file)
        {
                printf("ERROR: Could not write to logfile %s, bailing!\n\n",Config->logpath.c_str());
                Exit(ERROR);
        }
}


void CheckRoot()
{
        if (geteuid() == 0)
        {
                printf("WARNING!!! You are running an irc server as ROOT!!! DO NOT DO THIS!!!\n\n");
                log(DEFAULT,"InspIRCd: startup: not starting with UID 0!");
                Exit(ERROR);
        }
}


void CheckDie()
{
        if (*Config->DieValue)
        {
                printf("WARNING: %s\n\n",Config->DieValue);
                log(DEFAULT,"Ut-Oh, somebody didn't read their config file: '%s'",Config->DieValue);
                exit(0);
        }
}

void LoadAllModules(InspIRCd* ServerInstance)
{
        /* We must load the modules AFTER initializing the socket engine, now */
        MODCOUNT = -1;
	char configToken[MAXBUF];
        for (int count = 0; count < Config->ConfValueEnum("module",&Config->config_f); count++)
        {
                Config->ConfValue("module","name",count,configToken,&Config->config_f);
                printf("Loading module... \033[1;32m%s\033[0m\n",configToken);
                if (!ServerInstance->LoadModule(configToken))
                {
                        log(DEFAULT,"Exiting due to a module loader error.");
                        printf("\nThere was an error loading a module: %s\n\nYou might want to do './inspircd start' instead of 'bin/inspircd'\n\n",ServerInstance->ModuleError());
                        Exit(0);
                }
        }
        log(DEFAULT,"Total loaded modules: %lu",(unsigned long)MODCOUNT+1);
}


