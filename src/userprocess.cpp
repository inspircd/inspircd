/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "configreader.h"
#include "users.h"
#include "modules.h"
#include "wildcard.h"
#include "xline.h"
#include "socketengine.h"
#include "command_parse.h"

void InspIRCd::FloodQuitUser(userrec* current)
{
	this->Log(DEFAULT,"Excess flood from: %s@%s", current->ident, current->host);
	this->SNO->WriteToSnoMask('f',"Excess flood from: %s%s%s@%s",
			current->registered == REG_ALL ? current->nick : "",
			current->registered == REG_ALL ? "!" : "", current->ident, current->host);
	current->SetWriteError("Excess flood");
	if (current->registered != REG_ALL)
	{
		XLines->add_zline(120,this->Config->ServerName,"Flood from unregistered connection",current->GetIPString());
		XLines->apply_lines(APPLY_ZLINES);
	}
}

void InspIRCd::ProcessUser(userrec* cu)
{
	int result = EAGAIN;

	if (cu->GetFd() == FD_MAGIC_NUMBER)
		return;

	if (this->Config->GetIOHook(cu->GetPort()))
	{
		int result2 = 0;
		int MOD_RESULT = 0;

		try
		{
			MOD_RESULT = this->Config->GetIOHook(cu->GetPort())->OnRawSocketRead(cu->GetFd(),ReadBuffer,sizeof(ReadBuffer),result2);
		}
		catch (CoreException& modexcept)
		{
			this->Log(DEBUG, "%s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
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
		result = cu->ReadData(ReadBuffer, sizeof(ReadBuffer));
	}

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
			if (ReadBuffer[checker] == 0)
				ReadBuffer[checker] = ' ';
		}

		if (result > 0)
			ReadBuffer[result] = '\0';

		current = cu;
		currfd = current->GetFd();

		// add the data to the users buffer
		if (result > 0)
		{
			if (!current->AddBuffer(ReadBuffer))
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

					if (current->flood && current->lines_in > current->flood)
						FloodQuitUser(current);
					else
					{
						current->WriteServ("NOTICE %s :Your previous line was too long and was not delivered (Over %d chars) Please shorten it.", current->nick, MAXBUF-2);
						current->recvq.clear();
					}
				}
				else
					FloodQuitUser(current);

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

				if (++current->lines_in > current->flood && current->flood)
				{
					FloodQuitUser(current);
					return;
				}

				if ((++floodlines > current->flood) && (current->flood != 0))
				{
					FloodQuitUser(current);
					return;
				}

				// use GetBuffer to copy single lines into the sanitized string
				std::string single_line = current->GetBuffer();
				current->bytes_in += single_line.length();
				current->cmds_in++;
				if (single_line.length() > MAXBUF - 2)	/* MAXBUF is 514 to allow for neccessary line terminators */
					single_line.resize(MAXBUF - 2); /* So to trim to 512 here, we use MAXBUF - 2 */

				EventHandler* old_comp = this->SE->GetRef(currfd);

				this->Parser->ProcessBuffer(single_line,current);
				/*
				 * look for the user's record in case it's changed... if theyve quit,
				 * we cant do anything more with their buffer, so bail.
				 * there used to be an ugly, slow loop here. Now we have a reference
				 * table, life is much easier (and FASTER)
				 */
				EventHandler* new_comp = this->SE->GetRef(currfd);

				if (new_comp != old_comp)
					return;
			}

			return;
		}

		if ((result == -1) && (errno != EAGAIN) && (errno != EINTR))
		{
			cu->SetWriteError(errno ? strerror(errno) : "EOF from client");
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
		cu->SetWriteError("Connection closed");
		return;
	}
}

/**
 * This function is called once a second from the mainloop.
 * It is intended to do background checking on all the user structs, e.g.
 * stuff like ping checks, registration timeouts, etc.
 */
void InspIRCd::DoBackgroundUserStuff(time_t TIME)
{
	/* Is it time yet? */
	if (TIME < next_call)
		return;
	else
	{
		/* Time we actually need to call this again */
		const time_t DUMMY_VALUE = 32768;
		next_call = TIME + DUMMY_VALUE;

		/* XXX: IT IS NOT SAFE TO USE AN ITERATOR HERE. DON'T EVEN THINK ABOUT IT. */
		for (unsigned long count2 = 0; count2 != this->local_users.size(); count2++)
		{
			if (count2 >= this->local_users.size())
				break;

			userrec* curr = this->local_users[count2];

			if (curr)
			{
				/*
				 * registration timeout -- didnt send USER/NICK/HOST
				 * in the time specified in their connection class.
				 */
				if ((TIME > curr->timeout) && (curr->registered != REG_ALL))
				{
					curr->muted = true;
					userrec::QuitUser(this, curr, "Registration timeout");
					continue;
				}
				else
				{
					if ((curr->registered != REG_ALL) && (next_call > (time_t)curr->timeout))
						next_call = curr->timeout;
				}

				/*
				 * user has signed on with USER/NICK/PASS, and dns has completed, all the modules
				 * say this user is ok to proceed, fully connect them.
				 */
				bool ready = AllModulesReportReady(curr);
				if ((TIME > curr->signon) && (curr->registered == REG_NICKUSER) && (ready))
				{
					if (!curr->dns_done)
					{
						curr->WriteServ("NOTICE Auth :*** Could not resolve your hostname: Request timed out; using your IP address (%s) instead.", curr->GetIPString());
						curr->dns_done = true;
					}
					this->stats->statsDnsBad++;
					curr->FullConnect();
					continue;
				}
				else
				{
					if ((curr->registered == REG_NICKUSER) && (ready) && (next_call > curr->signon))
						next_call = curr->signon;
				}

				if ((curr->dns_done) && (curr->registered == REG_NICKUSER) && (ready))
				{
					curr->FullConnect();
					continue;
				}
				else
				{
					if ((curr->registered == REG_NICKUSER) && (ready) && (next_call > curr->signon + this->Config->dns_timeout))
						next_call = curr->signon + this->Config->dns_timeout;
				}

				// It's time to PING this user. Send them a ping.
				if ((TIME > curr->nping) && (curr->registered == REG_ALL))
				{
					// This user didn't answer the last ping, remove them
					if (!curr->lastping)
					{
						/* Everybody loves boobies. */
						time_t time = this->Time(false) - (curr->nping - curr->pingmax);
						char message[MAXBUF];
						snprintf(message, MAXBUF, "Ping timeout: %ld second%s", (long)time, time > 1 ? "s" : "");
						curr->muted = true;
						curr->lastping = 1;
						curr->nping = TIME+curr->pingmax;
						userrec::QuitUser(this, curr, message);
						continue;
					}
					curr->Write("PING :%s",this->Config->ServerName);
					curr->lastping = 0;
					curr->nping = TIME+curr->pingmax;
				}
				else
				{
					if ((curr->registered == REG_ALL) && (next_call > curr->nping))
						next_call = curr->nping;
				}
			}
		}

		/* If theres nothing to do, trigger in the next second, something might come up */
		time_t delta = next_call - TIME;
		if (delta == DUMMY_VALUE)
		{
			next_call = TIME + 1;
			delta = 1;
		}
	}
}
