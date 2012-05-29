/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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


#include "inspircd.h"
#include "inspsocket.h"
#include "xline.h"

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
		CrashState trace_handler(HERE_STR, curr);
		count2++;

		if (curr->quitting)
			continue;

		if (curr->CommandFloodPenalty || curr->eh->getSendQSize())
		{
			unsigned int rate = curr->MyClass->commandrate;
			if (curr->CommandFloodPenalty > rate)
				curr->CommandFloodPenalty -= rate;
			else
				curr->CommandFloodPenalty = 0;
			curr->eh->OnDataReady();
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
				if (curr->dns_done)
				{
					ModResult res;
					FIRST_MOD_RESULT(OnCheckReady, res, (curr));
					if (res != MOD_RES_DENY) {
						/* User has sent NICK/USER, modules are okay, DNS finished. */
						curr->FullConnect();
						continue;
					}
				}
				break;
		}

		if ((curr->registered & REG_NICKUSER) != REG_NICKUSER && (Time() > (time_t)(curr->age + curr->MyClass->registration_timeout)))
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

