/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core: libIRCDsnomasks */

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

bool SnomaskManager::SetLocalOnly(char letter, bool local)
{
	SnoList::iterator n = SnoMasks.find(letter);
	if (n != SnoMasks.end())
	{
		n->second->LocalOnly = local;
		return n->second->LocalOnly;
	}

	throw "snomask not found wtf";
}

bool SnomaskManager::EnableSnomask(char letter, const std::string &type, bool local)
{
	if (SnoMasks.find(letter) == SnoMasks.end())
	{
		Snomask *s = new Snomask(ServerInstance, letter, type, local);
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
	this->EnableSnomask('c',"CONNECT", true);			/* Local connect notices */
	this->EnableSnomask('C',"REMOTECONNECT");		/* Remote connect notices */
	this->EnableSnomask('q',"QUIT", true);			/* Local quit notices */
	this->EnableSnomask('Q',"REMOTEQUIT");			/* Remote quit notices */
	this->EnableSnomask('k',"KILL", true);			/* Kill notices */
	this->EnableSnomask('K',"REMOTEKILL");			/* Remote kill notices */
	this->EnableSnomask('l',"LINK");			/* Link notices */
	this->EnableSnomask('o',"OPER");			/* Oper up/down notices */
	this->EnableSnomask('A',"ANNOUNCEMENT");		/* formerly WriteOpers() - generic notices to all opers */
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

	/* Only opers can receive snotices, so we iterate the oper list */
	for (std::list<User*>::iterator i = ServerInstance->Users->all_opers.begin(); i != ServerInstance->Users->all_opers.end(); i++)
	{
		User* a = *i;
		if (IS_LOCAL(a) && a->IsModeSet('s') && a->IsModeSet('n') && a->IsNoticeMaskSet(MySnomask) && !a->quitting)
		{

			a->WriteServ("NOTICE %s :*** %s: %s", a->nick, this->Description.c_str(), this->LastMessage.c_str());
			if (Count > 1)
			{
				a->WriteServ("NOTICE %s :*** %s: (last message repeated %u times)", a->nick, this->Description.c_str(), Count);
			}
		}
	}

	if (!LocalOnly)
	{
		// XXX this is a bit ugly.
		std::string sno;
		sno += MySnomask;

		ServerInstance->PI->SendSNONotice(sno, this->Description + ": " + this->LastMessage);
		if (Count > 1)
			ServerInstance->PI->SendSNONotice(sno, this->Description + ": (last message repeated " + ConvToStr(Count) + " times)");
	}

	LastMessage = "";
	Count = 1;
}
