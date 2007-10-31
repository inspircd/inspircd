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

/* $Core: libIRCDuserprocess */

#include "inspircd.h"
#include "wildcard.h"
#include "xline.h"
#include "socketengine.h"
#include "command_parse.h"

void FloodQuitUserHandler::Call(User* current)
{
	Server->Log(DEFAULT,"Excess flood from: %s@%s", current->ident, current->host);
	Server->SNO->WriteToSnoMask('f',"Excess flood from: %s%s%s@%s",
			current->registered == REG_ALL ? current->nick : "",
			current->registered == REG_ALL ? "!" : "", current->ident, current->host);
	User::QuitUser(Server, current, "Excess flood");

	if (current->registered != REG_ALL)
	{
		ZLine* zl = new ZLine(Server, Server->Time(), 0, Server->Config->ServerName, "Flood from unregistered connection", current->GetIPString());
		if (Server->XLines->AddLine(zl,NULL))
			Server->XLines->ApplyLines();
		else
			delete zl;
	}
}

void ProcessUserHandler::Call(User* cu)
{
	int result = EAGAIN;

	if (cu->GetFd() == FD_MAGIC_NUMBER)
		return;

	char* ReadBuffer = Server->GetReadBuffer();

	if (Server->Config->GetIOHook(cu->GetPort()))
	{
		int result2 = 0;
		int MOD_RESULT = 0;

		try
		{
			MOD_RESULT = Server->Config->GetIOHook(cu->GetPort())->OnRawSocketRead(cu->GetFd(),ReadBuffer,Server->Config->NetBufferSize,result2);
		}
		catch (CoreException& modexcept)
		{
			Server->Log(DEBUG, "%s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
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
		User *current;
		int currfd;

		Server->stats->statsRecv += result;
		/*
		 * perform a check on the raw buffer as an array (not a string!) to remove
		 * character 0 which is illegal in the RFC - replace them with spaces.
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
					if (current->MyClass)
					{
						// Make sure they arn't flooding long lines.
						if (Server->Time() > current->reset_due)
						{
							current->reset_due = Server->Time() + current->MyClass->GetThreshold();
							current->lines_in = 0;
						}

						current->lines_in++;

						if (current->MyClass->GetFlood() && current->lines_in > current->MyClass->GetFlood())
							Server->FloodQuitUser(current);
						else
						{
							current->WriteServ("NOTICE %s :Your previous line was too long and was not delivered (Over %d chars) Please shorten it.", current->nick, MAXBUF-2);
							current->recvq.clear();
						}
					}
				}
				else
					Server->FloodQuitUser(current);

				return;
			}

			/* If user is over penalty, dont process here, just build up */
			if (!current->OverPenalty)
				Server->Parser->DoLines(current);

			return;
		}

		if ((result == -1) && (errno != EAGAIN) && (errno != EINTR))
		{
			User::QuitUser(Server, cu, errno ? strerror(errno) : "EOF from client");
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
		User::QuitUser(Server, cu, "Connection closed");
		return;
	}
}

/**
 * This function is called once a second from the mainloop.
 * It is intended to do background checking on all the user structs, e.g.
 * stuff like ping checks, registration timeouts, etc.
 */
void InspIRCd::DoBackgroundUserStuff()
{
	/*
	 * loop over all local users..
	 */
	for (std::vector<User*>::iterator count2 = local_users.begin(); count2 != local_users.end(); count2++)
	{
		User *curr = *count2;

		if (curr->Penalty)
		{
			curr->Penalty--;
			if (curr->Penalty < 10)
				Parser->DoLines(curr, true);
		}

		if (curr->OverPenalty)
		{
			if (curr->sendq.empty())
				curr->OverPenalty = false;
		}

		if ((curr->registered != REG_ALL) && (TIME > curr->timeout))
		{
			/*
			 * registration timeout -- didnt send USER/NICK/HOST
			 * in the time specified in their connection class.
			 */
			curr->muted = true;
			User::QuitUser(this, curr, "Registration timeout");
			continue;
		}

		/*
		 * `ready` means that the user has provided NICK/USER(/PASS), and all modules agree
		 * that the user is okay to proceed. The one thing we are then waiting for now is DNS...
		 */
		bool ready = ((curr->registered == REG_NICKUSER) && AllModulesReportReady(curr));

		if (ready)
		{
			if (curr->dns_done)
			{
				/* DNS passed, connect the user */
				curr->FullConnect();
				continue;
			}
		}

		// It's time to PING this user. Send them a ping.
		if ((TIME > curr->nping) && (curr->registered == REG_ALL))
		{
			// This user didn't answer the last ping, remove them
			if (!curr->lastping)
			{
				time_t time = this->Time(false) - (curr->nping - curr->MyClass->GetPingTime());
				char message[MAXBUF];
				snprintf(message, MAXBUF, "Ping timeout: %ld second%s", (long)time, time > 1 ? "s" : "");
				curr->muted = true;
				curr->lastping = 1;
				curr->nping = TIME + curr->MyClass->GetPingTime();
				User::QuitUser(this, curr, message);
				continue;
			}
			curr->Write("PING :%s",this->Config->ServerName);
			curr->lastping = 0;
			curr->nping = TIME  +curr->MyClass->GetPingTime();
		}
	}
}

