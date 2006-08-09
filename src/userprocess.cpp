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
#include "configreader.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <time.h>
#include <string>
#include <ext/hash_map>
#include <map>
#include <sstream>
#include <vector>
#include <deque>
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
#include "helperfuncs.h"
#include "hashcomp.h"
#include "socketengine.h"
#include "userprocess.h"
#include "typedefs.h"
#include "command_parse.h"
#include "cull_list.h"

extern struct sockaddr_in client,server;
extern socklen_t length;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
extern time_t TIME;
extern time_t OLDTIME;
extern std::vector<userrec*> local_users;
char data[65536];

extern user_hash clientlist;
extern chan_hash chanlist;

void InspIRCd::ProcessUser(userrec* cu)
{
	int result = EAGAIN;

	if (cu->fd == FD_MAGIC_NUMBER)
		return;

	log(DEBUG,"Processing user with fd %d",cu->fd);

	if (this->Config->GetIOHook(cu->GetPort()))
	{
		int result2 = 0;
		int MOD_RESULT = 0;

		try
		{
			MOD_RESULT = this->Config->GetIOHook(cu->GetPort())->OnRawSocketRead(cu->fd,data,65535,result2);
			log(DEBUG,"Data result returned by module: %d",MOD_RESULT);
		}
		catch (ModuleException& modexcept)
		{
			log(DEBUG,"Module exception caught: %s",modexcept.GetReason());
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
		userrec *current;
		int currfd;
		int floodlines = 0;

		this->stats->statsRecv += result;
		/*
		 * perform a check on the raw buffer as an array (not a string!) to remove
		 * character 0 which is illegal in the RFC - replace them with spaces.
		 * XXX - no garauntee there's not \0's in the middle of the data,
		 *       and no reason for it to be terminated either. -- Om
		 */

		for (int checker = 0; checker < result; checker++)
		{
			if (data[checker] == 0)
				data[checker] = ' ';
		}

		if (result > 0)
			data[result] = '\0';

		current = cu;
		currfd = current->fd;

		// add the data to the users buffer
		if (result > 0)
		{
			if (!current->AddBuffer(data))
			{
				// AddBuffer returned false, theres too much data in the user's buffer and theyre up to no good.
				if (current->registered == REG_ALL)
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
						userrec::QuitUser(current,"Excess flood");
						return;
					}
					else
					{
						current->WriteServ("NOTICE %s :Your previous line was too long and was not delivered (Over 512chars) Please shorten it.", current->nick);
						current->recvq = "";
					}
				}
				else
				{
					WriteOpers("*** Excess flood from %s",current->GetIPString());
					log(DEFAULT,"Excess flood from: %s",current->GetIPString());
					add_zline(120,this->Config->ServerName,"Flood from unregistered connection",current->GetIPString());
					apply_lines(APPLY_ZLINES);
				}

				return;
			}

			if (current->recvq.length() > (unsigned)this->Config->NetBufferSize)
			{
				if (current->registered == REG_ALL)
				{
					userrec::QuitUser(current,"RecvQ exceeded");
				}
				else
				{
					WriteOpers("*** Excess flood from %s",current->GetIPString());
					log(DEFAULT,"Excess flood from: %s",current->GetIPString());
					add_zline(120,this->Config->ServerName,"Flood from unregistered connection",current->GetIPString());
					apply_lines(APPLY_ZLINES);
				}

				return;
			}

			// while there are complete lines to process...
			while (current->BufferIsReady())
			{
				if (TIME > current->reset_due)
				{
					current->reset_due = TIME + current->threshold;
					current->lines_in = 0;
				}

				if (++current->lines_in > current->flood)
				{
					log(DEFAULT,"Excess flood from: %s!%s@%s",current->nick,current->ident,current->host);
					WriteOpers("*** Excess flood from: %s!%s@%s",current->nick,current->ident,current->host);
					userrec::QuitUser(current,"Excess flood");
					return;
				}

				if ((++floodlines > current->flood) && (current->flood != 0))
				{
					if (current->registered == REG_ALL)
					{
						log(DEFAULT,"Excess flood from: %s!%s@%s",current->nick,current->ident,current->host);
						WriteOpers("*** Excess flood from: %s!%s@%s",current->nick,current->ident,current->host);
						userrec::QuitUser(current,"Excess flood");
					}
					else
					{
						add_zline(120,this->Config->ServerName,"Flood from unregistered connection",current->GetIPString());
						apply_lines(APPLY_ZLINES);
					}

					return;
				}

				// use GetBuffer to copy single lines into the sanitized string
				std::string single_line = current->GetBuffer();
				current->bytes_in += single_line.length();
				current->cmds_in++;
				if (single_line.length() > 512)
					single_line.resize(512);

				userrec* old_comp = this->fd_ref_table[currfd];

				this->Parser->ProcessBuffer(single_line,current);
				/*
				 * look for the user's record in case it's changed... if theyve quit,
				 * we cant do anything more with their buffer, so bail.
				 * there used to be an ugly, slow loop here. Now we have a reference
				 * table, life is much easier (and FASTER)
				 */
				userrec* new_comp = this->fd_ref_table[currfd];
				if ((currfd < 0) || (!this->fd_ref_table[currfd]) || (old_comp != new_comp))
				{
					return;
				}
				else
				{
					/* The user is still here, flush their buffer */
					current->FlushWriteBuf();
				}
			}

			return;
		}

		if ((result == -1) && (errno != EAGAIN) && (errno != EINTR))
		{
			log(DEBUG,"killing: %s",cu->nick);
			userrec::QuitUser(cu,strerror(errno));
			return;
		}
	}

	// result EAGAIN means nothing read
	else if ((result == EAGAIN) || (result == -EAGAIN))
	{
		/* do nothing */
	}
	else if (result == 0)
	{
		log(DEBUG,"InspIRCd: Exited: %s",cu->nick);
		userrec::QuitUser(cu,"Client exited");
		log(DEBUG,"Bailing from client exit");
		return;
	}
}

void InspIRCd::DoSocketTimeouts(time_t TIME)
{
	unsigned int numsockets = this->module_sockets.size();
	SocketEngine* SE = this->SE;

	for (std::vector<InspSocket*>::iterator a = this->module_sockets.begin(); a < this->module_sockets.end(); a++)
	{
		InspSocket* s = (InspSocket*)*a;
		if ((s) && (s->GetFd() >= 0) && (s->GetFd() < MAX_DESCRIPTORS) && (this->socket_ref[s->GetFd()] != NULL) && (s->Timeout(TIME)))
		{
			log(DEBUG,"userprocess.cpp: Socket poll returned false, close and bail");
			this->socket_ref[s->GetFd()] = NULL;
			SE->DelFd(s->GetFd());
			this->module_sockets.erase(a);
			s->Close();
			DELETE(s);
			break;
		}

		if (this->module_sockets.size() != numsockets)
			break;
	}
}

/**
 * This function is called once a second from the mainloop.
 * It is intended to do background checking on all the user structs, e.g.
 * stuff like ping checks, registration timeouts, etc. This function is
 * also responsible for checking if InspSocket derived classes are timed out.
 */
void InspIRCd::DoBackgroundUserStuff(time_t TIME)
{
	CullList GlobalGoners;

	/* XXX: IT IS NOT SAFE TO USE AN ITERATOR HERE. DON'T EVEN THINK ABOUT IT. */
	for (unsigned long count2 = 0; count2 != local_users.size(); count2++)
	{
		if (count2 >= local_users.size())
			break;

		userrec* curr = local_users[count2];

		if (curr)
		{
			/*
			 * registration timeout -- didnt send USER/NICK/HOST
			 * in the time specified in their connection class.
			 */
			if (((unsigned)TIME > (unsigned)curr->timeout) && (curr->registered != REG_ALL))
			{
				log(DEBUG,"InspIRCd: registration timeout: %s",curr->nick);
				//ZapThisDns(curr->fd);
				GlobalGoners.AddItem(curr,"Registration timeout");
				continue;
			}
			/*
			 * user has signed on with USER/NICK/PASS, and dns has completed, all the modules
			 * say this user is ok to proceed, fully connect them.
			 */
			if ((TIME > curr->signon) && (curr->registered == REG_NICKUSER) && (AllModulesReportReady(curr)))
			{
				curr->dns_done = true;
				//ZapThisDns(curr->fd);
				this->stats->statsDnsBad++;
				curr->FullConnect(&GlobalGoners);
				continue;
			}
			if ((curr->dns_done) && (curr->registered == REG_NICKUSER) && (AllModulesReportReady(curr)))
			{
				log(DEBUG,"dns done, registered=3, and modules ready, OK");
				curr->FullConnect(&GlobalGoners);
				//ZapThisDns(curr->fd);
				continue;
			}
			// It's time to PING this user. Send them a ping.
			if ((TIME > curr->nping) && (curr->registered == REG_ALL))
			{
				// This user didn't answer the last ping, remove them
				if (!curr->lastping)
				{
					GlobalGoners.AddItem(curr,"Ping timeout");
					curr->lastping = 1;
					curr->nping = TIME+curr->pingmax;
					continue;
				}
				curr->Write("PING :%s",this->Config->ServerName);
				curr->lastping = 0;
				curr->nping = TIME+curr->pingmax;
			}

			/*
			 * We can flush the write buffer as the last thing we do, because if they
			 * match any of the above conditions its no use flushing their buffer anyway.
			 */
	
			curr->FlushWriteBuf();
			if (*curr->GetWriteError())
			{
				GlobalGoners.AddItem(curr,curr->GetWriteError());
				continue;
			}
		}

	}


	/* Remove all the queued users who are due to be quit, free memory used. */
	GlobalGoners.Apply();
}
