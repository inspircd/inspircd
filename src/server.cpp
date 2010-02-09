/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include <signal.h>
#include "exitcodes.h"
#include "inspircd.h"
#include "inspircd_version.h"

void InspIRCd::SignalHandler(int signal)
{
	if (signal == SIGHUP)
	{
		Rehash("Caught SIGHUP");
	}
	else if (signal == SIGTERM)
	{
		Exit(signal);
	}
}

void InspIRCd::Exit(int status)
{
#ifdef WINDOWS
	if (WindowsIPC)
		delete WindowsIPC;
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
		snprintf(versiondata,MAXBUF,"%s %s :%s [%s,%s,%s]",VERSION,Config->ServerName.c_str(),SYSTEM,REVISION,SE->GetName().c_str(),Config->sid.c_str());
	else
		snprintf(versiondata,MAXBUF,"InspIRCd-2.1 %s :%s",Config->ServerName.c_str(),Config->CustomVersion.c_str());
	return versiondata;
}

void InspIRCd::BuildISupport()
{
	// the neatest way to construct the initial 005 numeric, considering the number of configure constants to go in it...
	std::stringstream v;
	v << "WALLCHOPS WALLVOICES MODES=" << Config->Limits.MaxModes - 1 << " CHANTYPES=# PREFIX=" << this->Modes->BuildPrefixes() << " MAP MAXCHANNELS=" << Config->MaxChans << " MAXBANS=60 VBANLIST NICKLEN=" << Config->Limits.NickMax - 1;
	v << " CASEMAPPING=rfc1459 STATUSMSG=" << Modes->BuildPrefixes(false) << " CHARSET=ascii TOPICLEN=" << Config->Limits.MaxTopic - 1 << " KICKLEN=" << Config->Limits.MaxKick - 1 << " MAXTARGETS=" << Config->MaxTargets - 1;
	v << " AWAYLEN=" << Config->Limits.MaxAway - 1 << " CHANMODES=" << this->Modes->GiveModeList(MODETYPE_CHANNEL) << " FNC NETWORK=" << Config->Network << " MAXPARA=32 ELIST=MU";
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
	if (pos == 3)
	{
		// At pos 3, if we hit '9', we've run out of available UIDs, and need to reset to AAA..AAA.
		if (current_uid[pos] == '9')
		{
			for (int i = 3; i < (UUID_LENGTH - 1); i++)
			{
				current_uid[i] = 'A';
				pos  = UUID_LENGTH - 1;
			}
		}
		else
		{
			// Buf if we haven't, just keep incrementing merrily.
			current_uid[pos]++;
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
	static int curindex = -1;

	/*
	 * If -1, we're setting up. Copy SID into the first three digits, 9's to the rest, null term at the end
	 * Why 9? Well, we increment before we find, otherwise we have an unnecessary copy, and I want UID to start at AAA..AA
	 * and not AA..AB. So by initialising to 99999, we force it to rollover to AAAAA on the first IncrementUID call.
	 * Kind of silly, but I like how it looks.
	 *		-- w
	 */
	if (curindex == -1)
	{
		current_uid[0] = Config->sid[0];
		current_uid[1] = Config->sid[1];
		current_uid[2] = Config->sid[2];

		for (int i = 3; i < (UUID_LENGTH - 1); i++)
			current_uid[i] = '9';

		curindex = UUID_LENGTH - 2; // look at the end of the string now kthx, ignore null

		// Null terminator. Important.
		current_uid[UUID_LENGTH - 1] = '\0';
	}

	while (1)
	{
		// Add one to the last UID
		this->IncrementUID(curindex);

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



