/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
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
#include "xline.h"

#include "treesocket.h"
#include "treeserver.h"
#include "utils.h"
#include "link.h"
#include "main.h"
#include "../hash.h"

std::string TreeSocket::MyModules(int filter)
{
	std::vector<std::string> modlist = ServerInstance->Modules->GetAllModuleNames(filter);

	if (filter == VF_COMMON && proto_version != ProtocolVersion)
		CompatAddModules(modlist);

	std::string capabilities;
	sort(modlist.begin(),modlist.end());
	for (std::vector<std::string>::const_iterator i = modlist.begin(); i != modlist.end(); ++i)
	{
		if (i != modlist.begin())
			capabilities.push_back(proto_version > 1201 ? ' ' : ',');
		capabilities.append(*i);
		Module* m = ServerInstance->Modules->Find(*i);
		if (m && proto_version > 1201)
		{
			Version v = m->GetVersion();
			if (!v.link_data.empty())
			{
				capabilities.push_back('=');
				capabilities.append(v.link_data);
			}
		}
	}
	return capabilities;
}

static std::string BuildModeList(ModeType type)
{
	std::vector<std::string> modes;
	for(char c='A'; c <= 'z'; c++)
	{
		ModeHandler* mh = ServerInstance->Modes->FindMode(c, type);
		if (mh)
		{
			std::string mdesc = mh->name;
			mdesc.push_back('=');
			if (mh->GetPrefix())
				mdesc.push_back(mh->GetPrefix());
			if (mh->GetModeChar())
				mdesc.push_back(mh->GetModeChar());
			modes.push_back(mdesc);
		}
	}
	sort(modes.begin(), modes.end());
	irc::stringjoiner line(" ", modes, 0, modes.size() - 1);
	return line.GetJoined();
}

void TreeSocket::SendCapabilities(int phase)
{
	if (capab->capab_phase >= phase)
		return;

	if (capab->capab_phase < 1 && phase >= 1)
		WriteLine("CAPAB START " + ConvToStr(ProtocolVersion));

	capab->capab_phase = phase;
	if (phase < 2)
		return;

	char sep = proto_version > 1201 ? ' ' : ',';
	irc::sepstream modulelist(MyModules(VF_COMMON), sep);
	irc::sepstream optmodulelist(MyModules(VF_OPTCOMMON), sep);
	/* Send module names, split at 509 length */
	std::string item;
	std::string line = "CAPAB MODULES :";
	while (modulelist.GetToken(item))
	{
		if (line.length() + item.length() + 1 > 509)
		{
			this->WriteLine(line);
			line = "CAPAB MODULES :";
		}

		if (line != "CAPAB MODULES :")
			line.push_back(sep);

		line.append(item);
	}
	if (line != "CAPAB MODULES :")
		this->WriteLine(line);

	line = "CAPAB MODSUPPORT :";
	while (optmodulelist.GetToken(item))
	{
		if (line.length() + item.length() + 1 > 509)
		{
			this->WriteLine(line);
			line = "CAPAB MODSUPPORT :";
		}

		if (line != "CAPAB MODSUPPORT :")
			line.push_back(sep);

		line.append(item);
	}
	if (line != "CAPAB MODSUPPORT :")
		this->WriteLine(line);

	WriteLine("CAPAB CHANMODES :" + BuildModeList(MODETYPE_CHANNEL));
	WriteLine("CAPAB USERMODES :" + BuildModeList(MODETYPE_USER));

	std::string extra;
	/* Do we have sha256 available? If so, we send a challenge */
	if (Utils->ChallengeResponse && (ServerInstance->Modules->FindDataService<HashProvider>("hash/sha256")))
	{
		SetOurChallenge(ServerInstance->GenRandomStr(20));
		extra = " CHALLENGE=" + this->GetOurChallenge();
	}
	if (proto_version < 1202)
		extra += ServerInstance->Modes->FindMode('h', MODETYPE_CHANNEL) ? " HALFOP=1" : " HALFOP=0";

	this->WriteLine("CAPAB CAPABILITIES " /* Preprocessor does this one. */
			":NICKMAX="+ConvToStr(ServerInstance->Config->Limits.NickMax)+
			" CHANMAX="+ConvToStr(ServerInstance->Config->Limits.ChanMax)+
			" MAXMODES="+ConvToStr(ServerInstance->Config->Limits.MaxModes)+
			" IDENTMAX="+ConvToStr(ServerInstance->Config->Limits.IdentMax)+
			" MAXQUIT="+ConvToStr(ServerInstance->Config->Limits.MaxQuit)+
			" MAXTOPIC="+ConvToStr(ServerInstance->Config->Limits.MaxTopic)+
			" MAXKICK="+ConvToStr(ServerInstance->Config->Limits.MaxKick)+
			" MAXGECOS="+ConvToStr(ServerInstance->Config->Limits.MaxGecos)+
			" MAXAWAY="+ConvToStr(ServerInstance->Config->Limits.MaxAway)+
			" IP6SUPPORT=1"+
			" PROTOCOL="+ConvToStr(ProtocolVersion)+extra+
			" PREFIX="+ServerInstance->Modes->BuildPrefixes()+
			" CHANMODES="+ServerInstance->Modes->GiveModeList(MASK_CHANNEL)+
			" USERMODES="+ServerInstance->Modes->GiveModeList(MASK_USER)+
			// XXX: Advertise the presence or absence of m_globops in CAPAB CAPABILITIES.
			// Services want to know about it, and since m_globops was not marked as VF_(OPT)COMMON
			// in 2.0, we advertise it here to not break linking to previous versions.
			// Protocol version 1201 (1.2) does not have this issue because we advertise m_globops
			// to 1201 protocol servers irrespectively of its module flags.
			(ServerInstance->Modules->Find("m_globops.so") != NULL ? " GLOBOPS=1" : " GLOBOPS=0")+
			" SVSPART=1");

	this->WriteLine("CAPAB END");
}

/* Isolate and return the elements that are different between two comma seperated lists */
void TreeSocket::ListDifference(const std::string &one, const std::string &two, char sep,
		std::string& mleft, std::string& mright)
{
	std::set<std::string> values;
	irc::sepstream sepleft(one, sep);
	irc::sepstream sepright(two, sep);
	std::string item;
	while (sepleft.GetToken(item))
	{
		values.insert(item);
	}
	while (sepright.GetToken(item))
	{
		if (!values.erase(item))
		{
			mright.push_back(sep);
			mright.append(item);
		}
	}
	for(std::set<std::string>::iterator i = values.begin(); i != values.end(); ++i)
	{
		mleft.push_back(sep);
		mleft.append(*i);
	}
}

bool TreeSocket::Capab(const parameterlist &params)
{
	if (params.size() < 1)
	{
		this->SendError("Invalid number of parameters for CAPAB - Mismatched version");
		return false;
	}
	if (params[0] == "START")
	{
		capab->ModuleList.clear();
		capab->OptModuleList.clear();
		capab->CapKeys.clear();
		if (params.size() > 1)
			proto_version = atoi(params[1].c_str());
		SendCapabilities(2);
	}
	else if (params[0] == "END")
	{
		std::string reason;
		/* Compare ModuleList and check CapKeys */
		if ((this->capab->ModuleList != this->MyModules(VF_COMMON)) && (this->capab->ModuleList.length()))
		{
			std::string diffIneed, diffUneed;
			ListDifference(this->capab->ModuleList, this->MyModules(VF_COMMON), proto_version > 1201 ? ' ' : ',', diffIneed, diffUneed);
			if (diffIneed.length() || diffUneed.length())
			{
				reason = "Modules incorrectly matched on these servers.";
				if (diffIneed.length())
					reason += " Not loaded here:" + diffIneed;
				if (diffUneed.length())
					reason += " Not loaded there:" + diffUneed;
				this->SendError("CAPAB negotiation failed: "+reason);
				return false;
			}
		}
		if (this->capab->OptModuleList != this->MyModules(VF_OPTCOMMON) && this->capab->OptModuleList.length())
		{
			std::string diffIneed, diffUneed;
			ListDifference(this->capab->OptModuleList, this->MyModules(VF_OPTCOMMON), ' ', diffIneed, diffUneed);
			if (diffIneed.length() || diffUneed.length())
			{
				if (Utils->AllowOptCommon)
				{
					ServerInstance->SNO->WriteToSnoMask('l',
						"Optional module lists do not match, some commands may not work globally.%s%s%s%s",
						diffIneed.length() ? " Not loaded here:" : "", diffIneed.c_str(),
						diffUneed.length() ? " Not loaded there:" : "", diffUneed.c_str());
				}
				else
				{
					reason = "Optional modules incorrectly matched on these servers, and options::allowmismatch not set.";
					if (diffIneed.length())
						reason += " Not loaded here:" + diffIneed;
					if (diffUneed.length())
						reason += " Not loaded there:" + diffUneed;
					this->SendError("CAPAB negotiation failed: "+reason);
					return false;
				}
			}
		}

		if (this->capab->CapKeys.find("PROTOCOL") == this->capab->CapKeys.end())
		{
			reason = "Protocol version not specified";
		}
		else
		{
			proto_version = atoi(capab->CapKeys.find("PROTOCOL")->second.c_str());
			if (proto_version < MinCompatProtocol)
			{
				reason = "Server is using protocol version " + ConvToStr(proto_version) +
					" which is too old to link with this server (version " + ConvToStr(ProtocolVersion)
					+ (ProtocolVersion != MinCompatProtocol ? ", links with " + ConvToStr(MinCompatProtocol) + " and above)" : ")");
			}
		}

		if(this->capab->CapKeys.find("PREFIX") != this->capab->CapKeys.end() && this->capab->CapKeys.find("PREFIX")->second != ServerInstance->Modes->BuildPrefixes())
			reason = "One or more of the prefixes on the remote server are invalid on this server.";

		if (!capab->ChanModes.empty())
		{
			if (capab->ChanModes != BuildModeList(MODETYPE_CHANNEL))
			{
				std::string diffIneed, diffUneed;
				ListDifference(capab->ChanModes, BuildModeList(MODETYPE_CHANNEL), ' ', diffIneed, diffUneed);
				if (diffIneed.length() || diffUneed.length())
				{
					reason = "Channel modes not matched on these servers.";
					if (diffIneed.length())
						reason += " Not loaded here:" + diffIneed;
					if (diffUneed.length())
						reason += " Not loaded there:" + diffUneed;
				}
			}
		}
		else if (this->capab->CapKeys.find("CHANMODES") != this->capab->CapKeys.end())
		{
			if (this->capab->CapKeys.find("CHANMODES")->second != ServerInstance->Modes->GiveModeList(MASK_CHANNEL))
				reason = "One or more of the channel modes on the remote server are invalid on this server.";
		}

		if (!capab->UserModes.empty())
		{
			if (capab->UserModes != BuildModeList(MODETYPE_USER))
			{
				std::string diffIneed, diffUneed;
				ListDifference(capab->UserModes, BuildModeList(MODETYPE_USER), ' ', diffIneed, diffUneed);
				if (diffIneed.length() || diffUneed.length())
				{
					reason = "User modes not matched on these servers.";
					if (diffIneed.length())
						reason += " Not loaded here:" + diffIneed;
					if (diffUneed.length())
						reason += " Not loaded there:" + diffUneed;
				}
			}
		}
		else if (this->capab->CapKeys.find("USERMODES") != this->capab->CapKeys.end())
		{
			if (this->capab->CapKeys.find("USERMODES")->second != ServerInstance->Modes->GiveModeList(MASK_USER))
				reason = "One or more of the user modes on the remote server are invalid on this server.";
		}

		/* Challenge response, store their challenge for our password */
		std::map<std::string,std::string>::iterator n = this->capab->CapKeys.find("CHALLENGE");
		if (Utils->ChallengeResponse && (n != this->capab->CapKeys.end()) && (ServerInstance->Modules->FindDataService<HashProvider>("hash/sha256")))
		{
			/* Challenge-response is on now */
			this->SetTheirChallenge(n->second);
			if (!this->GetTheirChallenge().empty() && (this->LinkState == CONNECTING))
			{
				this->SendCapabilities(2);
				this->WriteLine("SERVER "+ServerInstance->Config->ServerName+" "+this->MakePass(capab->link->SendPass, capab->theirchallenge)+" 0 "+ServerInstance->Config->GetSID()+" :"+ServerInstance->Config->ServerDesc);
			}
		}
		else
		{
			/* They didnt specify a challenge or we don't have m_sha256.so, we use plaintext */
			if (this->LinkState == CONNECTING)
			{
				this->SendCapabilities(2);
				this->WriteLine("SERVER "+ServerInstance->Config->ServerName+" "+capab->link->SendPass+" 0 "+ServerInstance->Config->GetSID()+" :"+ServerInstance->Config->ServerDesc);
			}
		}

		if (reason.length())
		{
			this->SendError("CAPAB negotiation failed: "+reason);
			return false;
		}
	}
	else if ((params[0] == "MODULES") && (params.size() == 2))
	{
		if (!capab->ModuleList.length())
		{
			capab->ModuleList = params[1];
		}
		else
		{
			capab->ModuleList.push_back(proto_version > 1201 ? ' ' : ',');
			capab->ModuleList.append(params[1]);
		}
	}
	else if ((params[0] == "MODSUPPORT") && (params.size() == 2))
	{
		if (!capab->OptModuleList.length())
		{
			capab->OptModuleList = params[1];
		}
		else
		{
			capab->OptModuleList.push_back(' ');
			capab->OptModuleList.append(params[1]);
		}
	}
	else if ((params[0] == "CHANMODES") && (params.size() == 2))
	{
		capab->ChanModes = params[1];
	}
	else if ((params[0] == "USERMODES") && (params.size() == 2))
	{
		capab->UserModes = params[1];
	}
	else if ((params[0] == "CAPABILITIES") && (params.size() == 2))
	{
		irc::tokenstream capabs(params[1]);
		std::string item;
		while (capabs.GetToken(item))
		{
			/* Process each key/value pair */
			std::string::size_type equals = item.find('=');
			if (equals != std::string::npos)
			{
				std::string var = item.substr(0, equals);
				std::string value = item.substr(equals+1, item.length());
				capab->CapKeys[var] = value;
			}
		}
	}
	return true;
}

