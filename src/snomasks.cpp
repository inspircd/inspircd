/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Adam <Adam@anope.org>
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

unsigned int SnomaskManager::pos;
std::map<std::string, Snomask *> SnomaskManager::snomasksByName;
std::map<unsigned int, Snomask *> SnomaskManager::snomasksByPos;

Snomask
	SnomaskManager::connect("CONNECT"),
	SnomaskManager::quit("QUIT"),
	SnomaskManager::kill("KILL"),
	SnomaskManager::oper("OPER"),
	SnomaskManager::announcement("ANNOUNCEMENT"),
	SnomaskManager::debug("DEBUG"),
	SnomaskManager::xline("XLINE"),
	SnomaskManager::stats("STATS");

std::vector<bool>::reference Snomasks::operator[](size_t idx)
{
	if (snomasks.size() <= idx)
		snomasks.resize(idx + 1);
	return snomasks[idx];
}

bool Snomasks::operator[](size_t idx) const
{
	if (idx >= snomasks.size())
		return false;
	return snomasks[idx];
}

bool Snomasks::none() const
{
	for (unsigned i = 0; i < snomasks.size(); ++i)
		if (snomasks[i])
			return false;
	return true;
}

Snomask::Snomask(const std::string &Name) : LastRemote(false), Count(0), name(Name), pos(0)
{
	SnomaskManager::RegisterSnomask(this);
}

Snomask::~Snomask()
{
	SnomaskManager::UnregisterSnomask(this);
}

void Snomask::SendMessage(const std::string& message, bool remote)
{
	if (!ServerInstance->Config->NoSnoticeStack && message == LastMessage && remote == LastRemote)
	{
		Count++;
		return;
	}

	this->Flush();

	std::string desc = (remote ? "REMOTE" : "") + name;
	ModResult MOD_RESULT;
	FIRST_MOD_RESULT(OnSendSnotice, MOD_RESULT, (this, desc, message));
	if (MOD_RESULT == MOD_RES_DENY)
		return;

	this->Send(remote, desc, message);
	
	LastMessage = message;
	LastRemote = remote;
	Count++;
}

void Snomask::Flush()
{
	if (Count > 1)
	{
		std::string desc = (LastRemote ? "REMOTE" : "") + name;
		std::string msg = "(last message repeated " + ConvToStr(Count) + " times)";

		FOREACH_MOD(OnSendSnotice, (this, desc, msg));
		this->Send(LastRemote, desc, msg);
	}

	LastMessage.clear();
	Count = 0;
}

void Snomask::Send(bool remote, const std::string& desc, const std::string& msg)
{
	std::string log = desc + ": " + msg;
	ServerInstance->Logs->Log("snomask", LOG_DEFAULT, log);

	std::string finalmsg = "*** " + log;
	/* Only opers can receive snotices, so we iterate the oper list */
	const std::list<User*>& opers = ServerInstance->Users->all_opers;
	for (std::list<User*>::const_iterator i = opers.begin(); i != opers.end(); ++i)
	{
		User* user = *i;
		// IsNoticeMaskSet() returns false for opers who aren't +s, no need to check for it seperately
		if (IS_LOCAL(user) && user->IsNoticeMaskSet(this, remote))
			user->WriteNotice(finalmsg);
	}
}

void SnomaskManager::RegisterSnomask(Snomask *sno)
{
	if (snomasksByName[sno->name])
		throw ModuleException("Snomask " + sno->name + " already exists");

	snomasksByName[sno->name] = sno;

	sno->pos = pos;
	snomasksByPos[pos] = sno;
	pos += 2; // One for the local snomask, one for the remote
}

void SnomaskManager::UnregisterSnomask(Snomask *sno)
{
	snomasksByName.erase(sno->name);
	snomasksByPos.erase(sno->pos);
}

Snomask* SnomaskManager::FindSnomaskByName(const std::string &name)
{
	std::map<std::string, Snomask *>::iterator it = snomasksByName.find(name);
	if (it != snomasksByName.end())
		return it->second;
	return NULL;
}

Snomask* SnomaskManager::FindSnomaskByPos(unsigned int p)
{
	std::map<unsigned int, Snomask *>::iterator it = snomasksByPos.find(p);
	if (it != snomasksByPos.end())
		return it->second;
	return NULL;
}

std::vector<Snomask*> SnomaskManager::GetSnomasks()
{
	std::vector<Snomask*> v;
	for (std::map<std::string, Snomask *>::iterator it = snomasksByName.begin(); it != snomasksByName.end(); ++it)
		v.push_back(it->second);
	return v;
}

void SnomaskManager::Write(int where, Snomask &sno, const std::string &text)
{
	sno.SendMessage(text, where & SNO_REMOTE);
	if (where & SNO_BROADCAST)
		ServerInstance->PI->SendSNONotice(sno, text);
}

void SnomaskManager::Write(int where, Snomask &sno, const char* text, ...)
{
	std::string textbuffer;
	VAFORMAT(textbuffer, text, text);
	Write(where, sno, textbuffer);
}

void SnomaskManager::FlushSnotices()
{
	for (std::map<std::string, Snomask *>::iterator it = snomasksByName.begin(); it != snomasksByName.end(); ++it)
		it->second->Flush();
}

