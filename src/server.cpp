/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
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


#include <signal.h>
#include "exitcodes.h"
#include "inspircd.h"

void InspIRCd::SignalHandler(int signal)
{
#ifdef _WIN32
	if (signal == SIGTERM)
#else
	if (signal == SIGHUP)
	{
		ServerInstance->SNO->WriteGlobalSno('a', "Rehashing due to SIGHUP");
		Rehash();
	}
	else if (signal == SIGTERM)
#endif
	{
		Exit(EXIT_STATUS_SIGTERM);
	}
}

void InspIRCd::Exit(int status)
{
#ifdef _WIN32
	SetServiceStopped(status);
#endif
	this->Cleanup();
	ServerInstance = NULL;
	delete this;
	exit (status);
}

void InspIRCd::Rehash(const std::string& uuid)
{
	if (!ServerInstance->ConfigThread)
	{
		ServerInstance->ConfigThread = new ConfigReaderThread(uuid);
		ServerInstance->Threads.Start(ServerInstance->ConfigThread);
	}
}

std::string InspIRCd::GetVersionString(bool getFullVersion)
{
	if (getFullVersion)
		return INSPIRCD_VERSION " " + Config->ServerName + " :" INSPIRCD_SYSTEM " [" INSPIRCD_SOCKETENGINE_NAME "," + Config->sid + "]";
	return INSPIRCD_BRANCH " " + Config->ServerName + " :" + Config->CustomVersion;
}

std::string UIDGenerator::GenerateSID(const std::string& servername, const std::string& serverdesc)
{
	unsigned int sid = 0;

	for (std::string::const_iterator i = servername.begin(); i != servername.end(); ++i)
		sid = 5 * sid + *i;
	for (std::string::const_iterator i = serverdesc.begin(); i != serverdesc.end(); ++i)
		sid = 5 * sid + *i;

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
	while (1)
	{
		// Add one to the last UID
		this->IncrementUID(UUID_LENGTH - 1);

		if (!ServerInstance->FindUUID(current_uid))
			break;

		/*
		 * It's in use. We need to try the loop again.
		 */
	}

	return current_uid;
}

void ISupportManager::Build()
{
	/**
	 * This is currently the neatest way we can build the initial ISUPPORT map. In
	 * the future we can use an initializer list here.
	 */
	std::map<std::string, std::string> tokens;

	tokens["AWAYLEN"] = ConvToStr(ServerInstance->Config->Limits.MaxAway);
	tokens["CASEMAPPING"] = "rfc1459";
	tokens["CHANLIMIT"] = InspIRCd::Format("#:%u", ServerInstance->Config->MaxChans);
	tokens["CHANMODES"] = ServerInstance->Modes->GiveModeList(MODETYPE_CHANNEL);
	tokens["CHANNELLEN"] = ConvToStr(ServerInstance->Config->Limits.ChanMax);
	tokens["CHANTYPES"] = "#";
	tokens["ELIST"] = "MU";
	tokens["KICKLEN"] = ConvToStr(ServerInstance->Config->Limits.MaxKick);
	tokens["MAXTARGETS"] = ConvToStr(ServerInstance->Config->MaxTargets);
	tokens["MODES"] = ConvToStr(ServerInstance->Config->Limits.MaxModes);
	tokens["NETWORK"] = ServerInstance->Config->Network;
	tokens["NICKLEN"] = ConvToStr(ServerInstance->Config->Limits.NickMax);
	tokens["PREFIX"] = ServerInstance->Modes->BuildPrefixes();
	tokens["STATUSMSG"] = ServerInstance->Modes->BuildPrefixes(false);
	tokens["TOPICLEN"] = ConvToStr(ServerInstance->Config->Limits.MaxTopic);
	tokens["VBANLIST"];

	// Modules can add new tokens and also edit or remove existing tokens
	FOREACH_MOD(On005Numeric, (tokens));

	// EXTBAN is a special case as we need to sort it and prepend a comma.
	std::map<std::string, std::string>::iterator extban = tokens.find("EXTBAN");
	if (extban != tokens.end())
	{
		std::sort(extban->second.begin(), extban->second.end());
		extban->second.insert(0, ",");
	}

	// Transform the map into a list of lines, ready to be sent to clients
	Numeric::Numeric numeric(RPL_ISUPPORT);
	unsigned int token_count = 0;
	cachedlines.clear();

	for (std::map<std::string, std::string>::const_iterator it = tokens.begin(); it != tokens.end(); ++it)
	{
		numeric.push(it->first);
		std::string& token = numeric.GetParams().back();

		// If this token has a value then append a '=' char after the name and then the value itself
		if (!it->second.empty())
			token.append(1, '=').append(it->second);

		token_count++;

		if (token_count % 13 == 12 || it == --tokens.end())
		{
			// Reached maximum number of tokens for this line or the current token
			// is the last one; finalize the line and store it for later use
			numeric.push("are supported by this server");
			cachedlines.push_back(numeric);
			numeric.GetParams().clear();
		}
	}
}

void ISupportManager::SendTo(LocalUser* user)
{
	for (std::vector<Numeric::Numeric>::const_iterator i = cachedlines.begin(); i != cachedlines.end(); ++i)
		user->WriteNumeric(*i);
}
