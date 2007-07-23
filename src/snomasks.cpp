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
#include <stdarg.h>
#include "configreader.h"
#include "users.h"
#include "snomasks.h"

SnomaskManager::SnomaskManager(InspIRCd* Instance) : ServerInstance(Instance)
{
	SnoMasks.clear();
	this->SetupDefaults();
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
			if (IS_LOCAL(a) && a->IsModeSet('s') && a->IsModeSet('n') && a->IsNoticeMaskSet(n->first))
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

void SnomaskManager::SetupDefaults()
{
	this->EnableSnomask('c',"CONNECT");			/* Local connect notices */
	this->EnableSnomask('C',"REMOTECONNECT");	/* Remote connect notices */
	this->EnableSnomask('q',"QUIT");			/* Local quit notices */
	this->EnableSnomask('Q',"REMOTEQUIT");		/* Remote quit notices */
	this->EnableSnomask('k',"KILL");			/* Kill notices */
	this->EnableSnomask('K',"REMOTEKILL");		/* Remote kill notices */
	this->EnableSnomask('l',"LINK");			/* Link notices */
	this->EnableSnomask('o',"OPER");			/* Oper up/down notices */
	this->EnableSnomask('d',"DEBUG");			/* Debug notices */
	this->EnableSnomask('x',"XLINE");			/* Xline notice (g/z/q/k/e) */
	this->EnableSnomask('t',"STATS");			/* Local or remote stats request */
	this->EnableSnomask('f',"FLOOD");			/* Flooding notices */
}

