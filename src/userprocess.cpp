/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
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
			unsigned int rate = curr->MyClass->commandrate;
			if (curr->CommandFloodPenalty > rate)
				curr->CommandFloodPenalty -= rate;
			else
				curr->CommandFloodPenalty = 0;
			curr->eh.OnDataReady();
		}

		switch (curr->registered)
		{
			case REG_ALL:
				if (Time() > curr->nping)
				{
					// This user didn't answer the last ping, remove them
					if (!curr->lastping)
					{
						time_t time = this->Time() - (curr->nping - curr->MyClass->pingtime);
						char message[MAXBUF];
						snprintf(message, MAXBUF, "Ping timeout: %ld second%s", (long)time, time > 1 ? "s" : "");
						curr->lastping = 1;
						curr->nping = Time() + curr->MyClass->pingtime;
						this->Users->QuitUser(curr, message);
						continue;
					}

					curr->Write("PING :%s",this->Config->ServerName.c_str());
					curr->lastping = 0;
					curr->nping = Time()  +curr->MyClass->pingtime;
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

		if (curr->registered != REG_ALL && (Time() > (time_t)(curr->age + curr->MyClass->registration_timeout)))
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

