/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core */

#include "inspircd.h"
#include "xline.h"
#include "socketengine.h"
#include "command_parse.h"

void FloodQuitUserHandler::Call(User* current)
{
	ServerInstance->Logs->Log("USERS",DEFAULT,"Excess flood from: %s@%s", current->ident.c_str(), current->host.c_str());
	ServerInstance->SNO->WriteToSnoMask('f',"Excess flood from: %s%s%s@%s",
			current->registered == REG_ALL ? current->nick.c_str() : "",
			current->registered == REG_ALL ? "!" : "", current->ident.c_str(), current->host.c_str());
	ServerInstance->Users->QuitUser(current, "Excess flood");

	if (current->registered != REG_ALL)
	{
		ZLine* zl = new ZLine(ServerInstance->Time(), 0, ServerInstance->Config->ServerName, "Flood from unregistered connection", current->GetIPString());
		if (ServerInstance->XLines->AddLine(zl,NULL))
			ServerInstance->XLines->ApplyLines();
		else
			delete zl;
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
	std::vector<LocalUser*>::reverse_iterator count2 = this->Users->local_users.rbegin();
	while (count2 != this->Users->local_users.rend())
	{
		LocalUser *curr = *count2;
		count2++;

		if (curr->quitting)
			continue;

		if (curr->CommandFloodPenalty)
		{
			unsigned int rate = curr->MyClass->GetCommandRate();
			if (curr->CommandFloodPenalty > rate)
				curr->CommandFloodPenalty -= rate;
			else
				curr->CommandFloodPenalty = 0;
			curr->eh.OnDataReady();
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

					curr->Write("PING :%s",this->Config->ServerName.c_str());
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

