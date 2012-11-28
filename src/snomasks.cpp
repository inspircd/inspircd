/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
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
#include <stdarg.h>

void SnomaskManager::FlushSnotices()
{
	for (int i=0; i < 26; i++)
		masks[i].Flush();
}

void SnomaskManager::EnableSnomask(char letter, const std::string &type)
{
	if (letter >= 'a' && letter <= 'z')
		masks[letter - 'a'].Description = type;
}

void SnomaskManager::WriteToSnoMask(char letter, const std::string &text)
{
	if (letter >= 'a' && letter <= 'z')
		masks[letter - 'a'].SendMessage(text, letter);
	if (letter >= 'A' && letter <= 'Z')
		masks[letter - 'A'].SendMessage(text, letter);
}

void SnomaskManager::WriteGlobalSno(char letter, const std::string& text)
{
	WriteToSnoMask(letter, text);
	letter = toupper(letter);
	ServerInstance->PI->SendSNONotice(std::string(1, letter), text);
}

void SnomaskManager::WriteToSnoMask(char letter, const char* text, ...)
{
	char textbuffer[MAXBUF];
	va_list argsPtr;

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	this->WriteToSnoMask(letter, std::string(textbuffer));
}

void SnomaskManager::WriteGlobalSno(char letter, const char* text, ...)
{
	char textbuffer[MAXBUF];
	va_list argsPtr;

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	this->WriteGlobalSno(letter, std::string(textbuffer));
}

SnomaskManager::SnomaskManager()
{
	EnableSnomask('c',"CONNECT");			/* Local connect notices */
	EnableSnomask('q',"QUIT");			/* Local quit notices */
	EnableSnomask('k',"KILL");			/* Kill notices */
	EnableSnomask('l',"LINK");			/* Linking notices */
	EnableSnomask('o',"OPER");			/* Oper up/down notices */
	EnableSnomask('a',"ANNOUNCEMENT");	/* formerly WriteOpers() - generic notices to all opers */
	EnableSnomask('d',"DEBUG");			/* Debug notices */
	EnableSnomask('x',"XLINE");			/* Xline notice (g/z/q/k/e) */
	EnableSnomask('t',"STATS");			/* Local or remote stats request */
	EnableSnomask('f',"FLOOD");			/* Flooding notices */
}

/*************************************************************************************/

void Snomask::SendMessage(const std::string &message, char mysnomask)
{
	if (ServerInstance->Config->NoSnoticeStack || message != LastMessage || mysnomask != LastLetter)
	{
		this->Flush();
		LastMessage = message;
		LastLetter = mysnomask;

		std::string desc = Description;
		if (desc.empty())
			desc = std::string("SNO-") + (char)tolower(mysnomask);
		if (isupper(mysnomask))
			desc = "REMOTE" + desc;
		ModResult MOD_RESULT;
		ServerInstance->Logs->Log("snomask", DEFAULT, "%s: %s", desc.c_str(), message.c_str());

		FIRST_MOD_RESULT(OnSendSnotice, MOD_RESULT, (mysnomask, desc, message));

		LastBlocked = (MOD_RESULT == MOD_RES_DENY);

		if (!LastBlocked)
		{
			/* Only opers can receive snotices, so we iterate the oper list */
			std::list<User*>::iterator i = ServerInstance->Users->all_opers.begin();

			while (i != ServerInstance->Users->all_opers.end())
			{
				User* a = *i;
				if (IS_LOCAL(a) && a->IsModeSet('s') && a->IsNoticeMaskSet(mysnomask) && !a->quitting)
				{
					a->WriteServ("NOTICE %s :*** %s: %s", a->nick.c_str(), desc.c_str(), message.c_str());
				}

				i++;
			}
		}
	}
	Count++;
}

void Snomask::Flush()
{
	if (Count > 1)
	{
		std::string desc = Description;
		if (desc.empty())
			desc = std::string("SNO-") + (char)tolower(LastLetter);
		if (isupper(LastLetter))
			desc = "REMOTE" + desc;
		std::string mesg = "(last message repeated "+ConvToStr(Count)+" times)";

		ServerInstance->Logs->Log("snomask", DEFAULT, "%s: %s", desc.c_str(), mesg.c_str());

		FOREACH_MOD(I_OnSendSnotice, OnSendSnotice(LastLetter, desc, mesg));

		if (!LastBlocked)
		{
			/* Only opers can receive snotices, so we iterate the oper list */
			std::list<User*>::iterator i = ServerInstance->Users->all_opers.begin();

			while (i != ServerInstance->Users->all_opers.end())
			{
				User* a = *i;
				if (IS_LOCAL(a) && a->IsModeSet('s') && a->IsNoticeMaskSet(LastLetter) && !a->quitting)
				{
					a->WriteServ("NOTICE %s :*** %s: %s", a->nick.c_str(), desc.c_str(), mesg.c_str());
				}

				i++;
			}
		}

	}
	LastMessage.clear();
	LastBlocked = false;
	Count = 0;
}
