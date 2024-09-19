/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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

void InspIRCd::HandleSignal(sig_atomic_t signal)
{
	switch (signal)
	{
		case SIGTERM:
			Exit(EXIT_FAILURE);

#ifndef _WIN32
		case SIGHUP:
			ServerInstance->SNO.WriteGlobalSno('a', "Rehashing due to SIGHUP");
			Rehash();
			break;
#endif

		default:
			ServerInstance->Logs.Debug("SIGNAL", "BUG: InspIRCd::SignalHandler: unknown signal: {}",
				signal);
			break;
	}
}

void InspIRCd::Exit(int status)
{
#ifdef _WIN32
	SetServiceStopped(status);
#endif
	this->Cleanup();
	ServerInstance = nullptr;
	delete this;
	exit(status);
}

void InspIRCd::Rehash(const std::string& uuid)
{
	if (!ConfigThread)
	{
		ConfigThread = std::make_unique<ConfigReaderThread>(uuid);
		ConfigThread->Start();
	}
}

std::string UIDGenerator::GenerateSID(const std::string& servername, const std::string& serverdesc)
{
	unsigned int sid = 0;

	for (const auto chr : servername)
		sid = 5 * sid + chr;

	for (const auto chr : serverdesc)
		sid = 5 * sid + chr;

	std::string sidstr = ConvToStr(sid % 1000);
	sidstr.insert(0, 3 - sidstr.length(), '0');
	return sidstr;
}

void UIDGenerator::IncrementUID(unsigned int pos)
{
	/*
	 * Okay. The rules for generating a UID go like this...
	 * -- > ABCDEFGHIJKLMNOPQRSTUVWXYZ --> 012345679 --> WRAP
	 * That is, we start at A. When we reach Z, we go to 0. At 9, we go to
	 * A again, in an iterative fashion.. so..
	 * AAA9 -> AABA, and so on. -- w00t
	 */

	// If we hit Z, wrap around to 0.
	if (current_uid[pos] == 'Z')
	{
		current_uid[pos] = '0';
	}
	else if (current_uid[pos] == '9')
	{
		/*
		 * Or, if we hit 9, wrap around to pos = 'A' and (pos - 1)++,
		 * e.g. A9 -> BA -> BB ..
		 */
		current_uid[pos] = 'A';
		if (pos == 3)
		{
			// At pos 3, if we hit '9', we've run out of available UIDs, and reset to AAA..AAA.
			return;
		}
		this->IncrementUID(pos - 1);
	}
	else
	{
		// Anything else, nobody gives a shit. Just increment.
		current_uid[pos]++;
	}
}

void UIDGenerator::init(const std::string& sid)
{
	/*
	 * Copy SID into the first three digits, 9's to the rest, null term at the end
	 * Why 9? Well, we increment before we find, otherwise we have an unnecessary copy, and I want UID to start at AAA..AA
	 * and not AA..AB. So by initialising to 99999, we force it to rollover to AAAAA on the first IncrementUID call.
	 * Kind of silly, but I like how it looks.
	 *		-- w
	 */

	current_uid.resize(UUID_LENGTH, '9');
	current_uid[0] = sid[0];
	current_uid[1] = sid[1];
	current_uid[2] = sid[2];
}

/*
 * Retrieve the next valid UUID that is free for this server.
 */
std::string UIDGenerator::GetUID()
{
	while (true)
	{
		// Add one to the last UID
		this->IncrementUID(UUID_LENGTH - 1);

		if (!ServerInstance->Users.FindUUID(current_uid))
			break;

		/*
		 * It's in use. We need to try the loop again.
		 */
	}

	return current_uid;
}

const std::string& Server::GetPublicName() const
{
	if (!ServerInstance->Config->HideServer.empty())
		return ServerInstance->Config->HideServer;
	return GetName();
}

size_t Server::GetOperCount() const
{
	return ServerInstance->Users.all_opers.size();
}

size_t Server::GetUserCount() const
{
	return ServerInstance->Users.LocalUserCount();
}

void Server::SendMetadata(const std::string& key, const std::string& data) const
{
	// Do nothing for the local server.
}

void Server::SendMetadata(const Extensible* ext, const std::string& key, const std::string& data) const
{
	// Do nothing for the local server.
}
