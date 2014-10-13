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
#include "inspircd_version.h"

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
	this->SendError("Exiting with status " + ConvToStr(status) + " (" + std::string(ExitCodes[status]) + ")");
	this->Cleanup();
	delete this;
	ServerInstance = NULL;
	exit (status);
}

void RehashHandler::Call(const std::string &reason)
{
	ServerInstance->SNO->WriteToSnoMask('a', "Rehashing config file %s %s",ServerConfig::CleanFilename(ServerInstance->ConfigFileName.c_str()), reason.c_str());
	ServerInstance->RehashUsersAndChans();
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

void InspIRCd::BuildISupport()
{
	// the neatest way to construct the initial 005 numeric, considering the number of configure constants to go in it...
	std::stringstream v;
	v << "WALLCHOPS WALLVOICES MODES=" << Config->Limits.MaxModes << " CHANTYPES=# PREFIX=" << this->Modes->BuildPrefixes() << " MAP MAXCHANNELS=" << Config->MaxChans << " MAXBANS=60 VBANLIST NICKLEN=" << Config->Limits.NickMax;
	v << " CASEMAPPING=rfc1459 STATUSMSG=" << Modes->BuildPrefixes(false) << " CHARSET=ascii TOPICLEN=" << Config->Limits.MaxTopic << " KICKLEN=" << Config->Limits.MaxKick << " MAXTARGETS=" << Config->MaxTargets;
	v << " AWAYLEN=" << Config->Limits.MaxAway << " CHANMODES=" << this->Modes->GiveModeList(MASK_CHANNEL) << " FNC NETWORK=" << Config->Network << " MAXPARA=32 ELIST=MU" << " CHANNELLEN=" << Config->Limits.ChanMax;
	Config->data005 = v.str();
	FOREACH_MOD(I_On005Numeric,On005Numeric(Config->data005));
	Config->Update005();
}

void InspIRCd::IncrementUID(int pos)
{
	/*
	 * Okay. The rules for generating a UID go like this...
	 * -- > ABCDEFGHIJKLMNOPQRSTUVWXYZ --> 012345679 --> WRAP
	 * That is, we start at A. When we reach Z, we go to 0. At 9, we go to
	 * A again, in an iterative fashion.. so..
	 * AAA9 -> AABA, and so on. -- w00t
	 */
	if ((pos == 3) && (current_uid[3] == '9'))
	{
		// At pos 3, if we hit '9', we've run out of available UIDs, and need to reset to AAA..AAA.
		for (int i = 3; i < UUID_LENGTH-1; i++)
		{
			current_uid[i] = 'A';
		}
	}
	else
	{
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
			this->IncrementUID(pos - 1);
		}
		else
		{
			// Anything else, nobody gives a shit. Just increment.
			current_uid[pos]++;
		}
	}
}

/*
 * Retrieve the next valid UUID that is free for this server.
 */
std::string InspIRCd::GetUID()
{
	static bool inited = false;

	/*
	 * If we're setting up, copy SID into the first three digits, 9's to the rest, null term at the end
	 * Why 9? Well, we increment before we find, otherwise we have an unnecessary copy, and I want UID to start at AAA..AA
	 * and not AA..AB. So by initialising to 99999, we force it to rollover to AAAAA on the first IncrementUID call.
	 * Kind of silly, but I like how it looks.
	 *		-- w
	 */
	if (!inited)
	{
		inited = true;
		current_uid[0] = Config->sid[0];
		current_uid[1] = Config->sid[1];
		current_uid[2] = Config->sid[2];

		for (int i = 3; i < (UUID_LENGTH - 1); i++)
			current_uid[i] = '9';

		// Null terminator. Important.
		current_uid[UUID_LENGTH - 1] = '\0';
	}

	while (1)
	{
		// Add one to the last UID
		this->IncrementUID(UUID_LENGTH - 2);

		if (this->FindUUID(current_uid))
		{
			/*
			 * It's in use. We need to try the loop again.
			 */
			continue;
		}

		return current_uid;
	}

	/* not reached. */
	return "";
}



