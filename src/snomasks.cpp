/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021-2025 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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

void SnomaskManager::FlushSnotices()
{
	for (auto& mask : masks)
		mask.Flush();
}

void SnomaskManager::EnableSnomask(char letter, const std::string& type)
{
	if (letter >= 'a' && letter <= 'z')
		masks[letter - 'a'].Description = type;
}

void SnomaskManager::WriteToSnoMask(char letter, const std::string& text)
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
	ServerInstance->PI->SendSNONotice(letter, text);
}

SnomaskManager::SnomaskManager()
{
	EnableSnomask('a', "ANNOUNCEMENT");
	EnableSnomask('c', "CONNECT");
	EnableSnomask('k', "KILL");
	EnableSnomask('o', "OPER");
	EnableSnomask('q', "QUIT");
	EnableSnomask('r', "REHASH");
}

bool SnomaskManager::IsSnomask(char ch)
{
	return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

bool SnomaskManager::IsSnomaskUsable(char ch) const
{
	return IsSnomask(ch) && !masks[tolower(ch) - 'a'].Description.empty();
}

void Snomask::SendMessage(const std::string& message, char letter)
{
	if ((!ServerInstance->Config->NoSnoticeStack) && (message == LastMessage) && (letter == LastLetter))
	{
		Count++;
		return;
	}

	this->Flush();

	std::string desc = GetDescription(letter);
	ModResult modres;
	FIRST_MOD_RESULT(OnSendSnotice, modres, (letter, desc, message));
	if (modres == MOD_RES_DENY)
		return;

	Snomask::Send(letter, desc, message);
	LastMessage = message;
	LastLetter = letter;
	Count++;
}

void Snomask::Flush()
{
	if (Count > 1)
	{
		std::string desc = GetDescription(LastLetter);
		std::string msg = INSP_FORMAT("(last message repeated {} times)", Count);

		FOREACH_MOD(OnSendSnotice, (LastLetter, desc, msg));
		Snomask::Send(LastLetter, desc, msg);
	}

	LastMessage.clear();
	Count = 0;
}

void Snomask::Send(char letter, const std::string& desc, const std::string& msg)
{
	ServerInstance->Logs.Normal(desc, msg);
	const std::string finalmsg = INSP_FORMAT("*** {}: {}", desc, msg);

	/* Only opers can receive snotices, so we iterate the oper list */
	for (auto* user : ServerInstance->Users.all_opers)
	{
		// IsNoticeMaskSet() returns false for opers who aren't +s, no need to check for it separately
		if (IS_LOCAL(user) && user->IsNoticeMaskSet(letter))
			user->WriteNotice(finalmsg);
	}
}

std::string Snomask::GetDescription(char letter) const
{
	std::string ret;
	if (isupper(letter))
		ret = "REMOTE";
	if (!Description.empty())
		ret += Description;
	else
		ret += INSP_FORMAT("SNO-{}", tolower(letter));
	return ret;
}
