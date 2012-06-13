/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2006-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2005-2007 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig McLure <craig@chatspike.net>
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
#include "xline.h"
#include "socketengine.h"
#include "command_parse.h"

void FloodQuitUserHandler::Call(User* current)
{
	Server->Logs->Log("USERS",DEFAULT,"Excess flood from: %s@%s", current->ident.c_str(), current->host.c_str());
	Server->SNO->WriteToSnoMask('f',"Excess flood from: %s%s%s@%s",
			current->registered == REG_ALL ? current->nick.c_str() : "",
			current->registered == REG_ALL ? "!" : "", current->ident.c_str(), current->host.c_str());
	Server->Users->QuitUser(current, "Excess flood");

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

	if (cu->GetIOHook())
	{
		int result2 = 0;
		int MOD_RESULT = 0;

		try
		{
			MOD_RESULT = cu->GetIOHook()->OnRawSocketRead(cu->GetFd(), ReadBuffer, Server->Config->NetBufferSize, result2);
		}
		catch (CoreException& modexcept)
		{
			Server->Logs->Log("USERS",DEBUG, "%s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
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
		result = cu->ReadData(ReadBuffer, Server->Config->NetBufferSize);
	}

	if ((result) && (result != -EAGAIN))
	{
		User *current;

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

		// add the data to the users buffer
		if (result > 0)
		{
			if (!current->AddBuffer(ReadBuffer))
			{
				// AddBuffer returned false, theres too much data in the user's buffer and theyre up to no good.
				Server->FloodQuitUser(current);
				return;
			}

			/* If user is over penalty, dont process here, just build up */
			if (current->Penalty < 10)
				Server->Parser->DoLines(current);

			return;
		}

		if ((result == -1) && (errno != EAGAIN) && (errno != EINTR))
		{
			Server->Users->QuitUser(cu, errno ? strerror(errno) : "EOF from client");
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
		Server->Users->QuitUser(cu, "Connection closed");
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
	for (std::vector<User*>::iterator count2 = this->Users->local_users.begin(); count2 != this->Users->local_users.end(); count2++)
	{
		User *curr = *count2;

		if (curr->quitting)
			continue;

		if (curr->Penalty)
		{
			curr->Penalty--;
			if (curr->Penalty < 10)
				Parser->DoLines(curr, true);
		}

		switch (curr->registered)
		{
			case REG_ALL:
				if (TIME > curr->nping)
				{
					// This user didn't answer the last ping, remove them
					if (!curr->lastping)
					{
						time_t time = this->Time() - (curr->nping - curr->MyClass->GetPingTime());
						char message[MAXBUF];
						snprintf(message, MAXBUF, "Ping timeout: %ld second%s", (long)time, time > 1 ? "s" : "");
						curr->lastping = 1;
						curr->nping = TIME + curr->MyClass->GetPingTime();
						this->Users->QuitUser(curr, message);
						continue;
					}

					curr->Write("PING :%s",this->Config->ServerName);
					curr->lastping = 0;
					curr->nping = TIME  +curr->MyClass->GetPingTime();
				}
				break;
			case REG_NICKUSER:
				if (AllModulesReportReady(curr) && curr->dns_done)
				{
					/* User has sent NICK/USER, modules are okay, DNS finished. */
					curr->FullConnect();
					continue;
				}
				break;
		}

		if (curr->registered != REG_ALL && (TIME > (curr->age + curr->MyClass->GetRegTimeout())))
		{
			/*
			 * registration timeout -- didnt send USER/NICK/HOST
			 * in the time specified in their connection class.
			 */
			this->Users->QuitUser(curr, "Registration timeout");
			continue;
		}
	}
}

