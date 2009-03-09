/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core */

#include "inspircd.h"
#include <stdarg.h>
#include "snomasks.h"

SnomaskManager::SnomaskManager(InspIRCd* Instance) : ServerInstance(Instance)
{
	SnoMasks.clear();
	this->SetupDefaults();
}

SnomaskManager::~SnomaskManager()
{
	for (std::map<char, Snomask *>::iterator i = SnoMasks.begin(); i != SnoMasks.end(); i++)
	{
		delete i->second;
	}
	SnoMasks.clear();
}

void SnomaskManager::FlushSnotices()
{
	for (std::map<char, Snomask *>::iterator i = SnoMasks.begin(); i != SnoMasks.end(); i++)
	{
		i->second->Flush();
	}
}

bool SnomaskManager::EnableSnomask(char letter, const std::string &type)
{
	if (SnoMasks.find(letter) == SnoMasks.end())
	{
		Snomask *s = new Snomask(ServerInstance, letter, type);
		SnoMasks[letter] = s;
		return true;
	}
	return false;
}

bool SnomaskManager::DisableSnomask(char letter)
{
	SnoList::iterator n = SnoMasks.find(letter);
	if (n != SnoMasks.end())
	{
		delete n->second; // destroy the snomask
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
		n->second->SendMessage(text);
		// XXX: Always try flush here. This removes snomask stacking effectively, as it's too annoying in it's present form. This may be reworked for RC3, or delayed until post-release.
		n->second->Flush();
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
	this->EnableSnomask('l',"LINK");			/* Linking notices */
	this->EnableSnomask('L',"REMOTELINK");			/* Remote linking notices */
	this->EnableSnomask('o',"OPER");			/* Oper up/down notices */
	this->EnableSnomask('O',"REMOTEOPER");			/* Remote oper up/down notices */
	this->EnableSnomask('A',"ANNOUNCEMENT");	/* formerly WriteOpers() - generic notices to all opers */
	this->EnableSnomask('d',"DEBUG");			/* Debug notices */
	this->EnableSnomask('x',"XLINE");			/* Xline notice (g/z/q/k/e) */
	this->EnableSnomask('t',"STATS");			/* Local or remote stats request */
	this->EnableSnomask('f',"FLOOD");			/* Flooding notices */
}

/*************************************************************************************/

void Snomask::SendMessage(const std::string &message)
{
	if (message != LastMessage)
	{
		this->Flush();
		LastMessage = message;
	}
	else
	{
		Count++;
	}
}

void Snomask::Flush()
{
	if (this->LastMessage.empty())
		return;

	ServerInstance->Logs->Log("snomask", DEFAULT, "%s: %s", this->Description.c_str(), this->LastMessage.c_str());
	if (Count > 1)
		ServerInstance->Logs->Log("snomask", DEFAULT, "%s: (last message repeated %u times)", this->Description.c_str(), Count);


	int MOD_RESULT = 0;
	char mysnomask = MySnomask;
	std::string desc = this->Description;

	FOREACH_RESULT(I_OnSendSnotice, OnSendSnotice(mysnomask, desc, this->LastMessage));

	if (MOD_RESULT != 1) // 1 blocks the message
	{
		/* Only opers can receive snotices, so we iterate the oper list */
		std::list<User*>::iterator i = ServerInstance->Users->all_opers.begin();

		while (i != ServerInstance->Users->all_opers.end())
		{
			User* a = *i;
			if (IS_LOCAL(a) && a->IsModeSet('s') && a->IsNoticeMaskSet(mysnomask) && !a->quitting)
			{

				a->WriteServ("NOTICE %s :*** %s: %s", a->nick.c_str(), desc.c_str(), this->LastMessage.c_str());
				if (Count > 1)
				{
					a->WriteServ("NOTICE %s :*** %s: (last message repeated %u times)", a->nick.c_str(), this->Description.c_str(), Count);
				}
			}

			i++;
		}
	}

	LastMessage = "";
	Count = 1;
}
