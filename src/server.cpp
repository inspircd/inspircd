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
		Rehash("Caught SIGHUP");
	}
	else if (signal == SIGTERM)
#endif
	{
		Exit(signal);
	}
}

void InspIRCd::Exit(int status)
{
#ifdef _WIN32
	SetServiceStopped(status);
#endif
	if (this)
	{
		this->SendError("Exiting with status " + ConvToStr(status) + " (" + std::string(ExitCodes[status]) + ")");
		this->Cleanup();
		delete this;
		ServerInstance = NULL;
	}
	exit (status);
}

void RehashHandler::Call(const std::string &reason)
{
	ServerInstance->SNO->WriteToSnoMask('a', "Rehashing config file %s %s",ServerConfig::CleanFilename(ServerInstance->ConfigFileName.c_str()), reason.c_str());
	FOREACH_MOD(I_OnGarbageCollect, OnGarbageCollect());
	if (!ServerInstance->ConfigThread)
	{
		ServerInstance->ConfigThread = new ConfigReaderThread("");
		ServerInstance->Threads->Start(ServerInstance->ConfigThread);
	}
}

std::string InspIRCd::GetVersionString(bool operstring)
{
	char versiondata[MAXBUF];
	if (operstring)
	{
		std::string sename = SE->GetName();
		snprintf(versiondata,MAXBUF,"%s %s :%s [%s,%s,%s]",VERSION, Config->ServerName.c_str(), SYSTEM,REVISION, sename.c_str(), Config->sid.c_str());
	}
	else
		snprintf(versiondata,MAXBUF,"%s %s :%s",BRANCH,Config->ServerName.c_str(),Config->CustomVersion.c_str());
	return versiondata;
}

const char InspIRCd::LogHeader[] =
	"Log started for " VERSION " (" REVISION ", " MODULE_INIT_STR ")"
	" - compiled on " SYSTEM;


std::string UIDGenerator::GenerateSID(const std::string& servername, const std::string& serverdesc)
{
	unsigned int sid = 0;

	for (std::string::const_iterator i = servername.begin(); i != servername.end(); ++i)
		sid = 5 * sid + *i;
	for (std::string::const_iterator i = serverdesc.begin(); i != serverdesc.end(); ++i)
		sid = 5 * sid + *i;

	std::string sidstr = ConvToStr(sid % 1000);
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

	current_uid[0] = sid[0];
	current_uid[1] = sid[1];
	current_uid[2] = sid[2];

	for (int i = 3; i < (UUID_LENGTH - 1); i++)
		current_uid[i] = '9';

	// Null terminator. Important.
	current_uid[UUID_LENGTH - 1] = '\0';
}

/*
 * Retrieve the next valid UUID that is free for this server.
 */
std::string UIDGenerator::GetUID()
{
	while (1)
	{
		// Add one to the last UID
		this->IncrementUID(UUID_LENGTH - 2);

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
	std::vector<std::string> lines;
	int token_count = 0;
	std::string line;

	tokens["AWAYLEN"] = ConvToStr(ServerInstance->Config->Limits.MaxAway);
	tokens["CASEMAPPING"] = "rfc1459";
	tokens["CHANMODES"] = ConvToStr(ServerInstance->Modes->GiveModeList(MASK_CHANNEL));
	tokens["CHANNELLEN"] = ConvToStr(ServerInstance->Config->Limits.ChanMax);
	tokens["CHANTYPES"] = "#";
	tokens["CHARSET"] = "ascii";
	tokens["ELIST"] = "MU";
	tokens["KICKLEN"] = ConvToStr(ServerInstance->Config->Limits.MaxKick);
	tokens["MAXBANS"] = "64"; // TODO: make this a config setting.
	tokens["MAXCHANNELS"] = ConvToStr(ServerInstance->Config->MaxChans);
	tokens["MAXPARA"] = ConvToStr(MAXPARAMETERS);
	tokens["MAXTARGETS"] = ConvToStr(ServerInstance->Config->MaxTargets);
	tokens["MODES"] = ConvToStr(ServerInstance->Config->Limits.MaxModes);
	tokens["NETWORK"] = ConvToStr(ServerInstance->Config->Network);
	tokens["NICKLEN"] = ConvToStr(ServerInstance->Config->Limits.NickMax);
	tokens["PREFIX"] = ServerInstance->Modes->BuildPrefixes();
	tokens["STATUSMSG"] = ServerInstance->Modes->BuildPrefixes(false);
	tokens["TOPICLEN"] = ConvToStr(ServerInstance->Config->Limits.MaxTopic);

	tokens["FNC"] = tokens["MAP"] = tokens["VBANLIST"] =
		tokens["WALLCHOPS"] = tokens["WALLVOICES"];

	FOREACH_MOD(I_On005Numeric, On005Numeric(tokens));

	// EXTBAN is a special case as we need to sort it and prepend a comma.
	std::map<std::string, std::string>::iterator extban = tokens.find("EXTBAN");
	if (extban != tokens.end())
	{
		sort(extban->second.begin(), extban->second.end());
		extban->second.insert(0, ",");
	}

	for (std::map<std::string, std::string>::iterator it = tokens.begin(); it != tokens.end(); it++)
	{
		line.append(it->first + (it->second.empty() ? " " : "=" + it->second + " "));
		token_count++;

		if (token_count % 13 == 12 || it == --tokens.end())
		{
			line.append(":are supported by this server");
			lines.push_back(line);
			line.clear();
		}
	}

	this->Lines = lines;
}

void ISupportManager::SendTo(LocalUser* user)
{
	for (std::vector<std::string>::iterator line = this->Lines.begin(); line != this->Lines.end(); line++)
		user->WriteNumeric(RPL_ISUPPORT, "%s %s", user->nick.c_str(), line->c_str());
}
