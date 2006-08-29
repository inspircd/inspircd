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

#include <string>
#include <vector>
#include <stdarg.h>
#include "configreader.h"
#include "users.h"
#include "modules.h"
#include "commands.h"
#include "xline.h"
#include "snomasks.h"

SnomaskManager::SnomaskManager(InspIRCd* Instance) : ServerInstance(Instance)
{
	SnoMasks.clear();
}

SnomaskManager::~SnomaskManager()
{
}

bool SnomaskManager::EnableSnomask(char letter, const std::string &type)
{
	if (SnoMasks.find(letter) == SnoMasks.end())
	{
		SnoMasks[letter] = type;
		return true;
	}
	return false;
}

bool SnomaskManager::DisableSnomask(char letter)
{
	SnoList::iterator n = SnoMasks.find(letter);
	if (n != SnoMasks.end())
	{
		SnoMasks.erase(n);
		return true;
	}
	return false;
}

void SnomaskManager::WriteToSnoMask(char letter, const std::string &text)
{
	/* Only send to snomask chars which are enabled */
	SnoList::iterator n = SnoMasks.find(letter);
	if (n != SnoMasks.end())
	{
		/* Only opers can receive snotices, so we iterate the oper list */
		for (std::vector<userrec*>::iterator i = ServerInstance->all_opers.begin(); i != ServerInstance->all_opers.end(); i++)
		{
			userrec* a = *i;
			if (IS_LOCAL(a) && a->modes[UM_SERVERNOTICE] && a->modes[UM_SNOMASK] && a->IsNoticeMaskSet(n->first))
			{
				/* send server notices to all with +ns */
				a->WriteServ("NOTICE %s :*** %s: %s",a->nick, n->second.c_str(), text.c_str());
			}
		}
	}
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

bool SnomaskManager::IsEnabled(char letter)
{
	return (SnoMasks.find(letter) != SnoMasks.end());
}

