/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "xline.h"

#include "treesocket.h"
#include "treeserver.h"
#include "utils.h"
#include "main.h"

/* $ModDep: m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */

std::string TreeSocket::MyModules(int filter)
{
	std::vector<std::string> modlist = this->ServerInstance->Modules->GetAllModuleNames(filter);

	if (filter == VF_COMMON && proto_version != ProtocolVersion)
		CompatAddModules(modlist);

	std::string capabilities;
	sort(modlist.begin(),modlist.end());
	for (unsigned int i = 0; i < modlist.size(); i++)
	{
		if (i)
			capabilities = capabilities + ",";
		capabilities = capabilities + modlist[i];
	}
	return capabilities;
}

void TreeSocket::SendCapabilities(int phase)
{
	if (capab_phase >= phase)
		return;

	if (capab_phase < 1 && phase >= 1)
		WriteLine("CAPAB START " + ConvToStr(ProtocolVersion));

	capab_phase = phase;
	if (phase < 2)
		return;

	irc::commasepstream modulelist(MyModules(VF_COMMON));
	irc::commasepstream optmodulelist(MyModules(VF_OPTCOMMON));
	/* Send module names, split at 509 length */
	std::string item;
	std::string line = "CAPAB MODULES ";
	while (modulelist.GetToken(item))
	{
		if (line.length() + item.length() + 1 > 509)
		{
			this->WriteLine(line);
			line = "CAPAB MODULES ";
		}

		if (line != "CAPAB MODULES ")
			line.append(",");

		line.append(item);
	}
	if (line != "CAPAB MODULES ")
		this->WriteLine(line);

	line = "CAPAB MODSUPPORT ";
	while (optmodulelist.GetToken(item))
	{
		if (line.length() + item.length() + 1 > 509)
		{
			this->WriteLine(line);
			line = "CAPAB MODSUPPORT ";
		}

		if (line != "CAPAB MODSUPPORT ")
			line.append(",");

		line.append(item);
	}
	if (line != "CAPAB MODSUPPORT ")
		this->WriteLine(line);


	int ip6 = 0;
#ifdef IPV6
	ip6 = 1;
#endif
	std::string extra;
	/* Do we have sha256 available? If so, we send a challenge */
	if (Utils->ChallengeResponse && (ServerInstance->Modules->Find("m_sha256.so")))
	{
		this->SetOurChallenge(RandString(20));
		extra = " CHALLENGE=" + this->GetOurChallenge();
	}

	this->WriteLine("CAPAB CAPABILITIES " /* Preprocessor does this one. */
			":NICKMAX="+ConvToStr(ServerInstance->Config->Limits.NickMax)+
			" HALFOP="+ConvToStr(ServerInstance->Config->AllowHalfop)+
			" CHANMAX="+ConvToStr(ServerInstance->Config->Limits.ChanMax)+
			" MAXMODES="+ConvToStr(ServerInstance->Config->Limits.MaxModes)+
			" IDENTMAX="+ConvToStr(ServerInstance->Config->Limits.IdentMax)+
			" MAXQUIT="+ConvToStr(ServerInstance->Config->Limits.MaxQuit)+
			" MAXTOPIC="+ConvToStr(ServerInstance->Config->Limits.MaxTopic)+
			" MAXKICK="+ConvToStr(ServerInstance->Config->Limits.MaxKick)+
			" MAXGECOS="+ConvToStr(ServerInstance->Config->Limits.MaxGecos)+
			" MAXAWAY="+ConvToStr(ServerInstance->Config->Limits.MaxAway)+
			" IP6NATIVE="+ConvToStr(ip6)+
			" IP6SUPPORT=1"+
			" PROTOCOL="+ConvToStr(ProtocolVersion)+extra+
			" PREFIX="+ServerInstance->Modes->BuildPrefixes()+
			" CHANMODES="+ServerInstance->Modes->GiveModeList(MASK_CHANNEL)+
			" USERMODES="+ServerInstance->Modes->GiveModeList(MASK_USER)+
			" SVSPART=1");

	this->WriteLine("CAPAB END");
}

/* Check a comma seperated list for an item */
bool TreeSocket::HasItem(const std::string &list, const std::string &item)
{
	irc::commasepstream seplist(list);
	std::string item2;

	while (seplist.GetToken(item2))
	{
		if (item2 == item)
			return true;
	}
	return false;
}

/* Isolate and return the elements that are different between two comma seperated lists */
std::string TreeSocket::ListDifference(const std::string &one, const std::string &two)
{
	irc::commasepstream list_one(one);
	std::string item;
	std::string result;
	while (list_one.GetToken(item))
	{
		if (!HasItem(two, item))
		{
			result.append(" ");
			result.append(item);
		}
	}
	return result;
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
		ModuleList.clear();
		OptModuleList.clear();
		CapKeys.clear();
		if (params.size() > 1)
			proto_version = atoi(params[1].c_str());
		SendCapabilities(2);
	}
	else if (params[0] == "END")
	{
		std::string reason;
		/* Compare ModuleList and check CapKeys */
		if ((this->ModuleList != this->MyModules(VF_COMMON)) && (this->ModuleList.length()))
		{
			std::string diffIneed = ListDifference(this->ModuleList, this->MyModules(VF_COMMON));
			std::string diffUneed = ListDifference(this->MyModules(VF_COMMON), this->ModuleList);
			if (diffIneed.length() == 0 && diffUneed.length() == 0)
			{
				reason = "Module list in CAPAB is not alphabetically ordered, cannot compare lists.";
			}
			else
			{
				reason = "Modules incorrectly matched on these servers.";
				if (diffIneed.length())
					reason += " Not loaded here:" + diffIneed;
				if (diffUneed.length())
					reason += " Not loaded there:" + diffUneed;
			}
			this->SendError("CAPAB negotiation failed: "+reason);
			return false;
		}
		if (this->OptModuleList != this->MyModules(VF_OPTCOMMON) && this->OptModuleList.length())
		{
			std::string diffIneed = ListDifference(this->OptModuleList, this->MyModules(VF_OPTCOMMON));
			std::string diffUneed = ListDifference(this->MyModules(VF_OPTCOMMON), this->OptModuleList);
			if (diffIneed.length() == 0 && diffUneed.length() == 0)
			{
				reason = "Optional Module list in CAPAB is not alphabetically ordered, cannot compare lists.";
			}
			else if (Utils->AllowOptCommon)
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

		if (this->CapKeys.find("PROTOCOL") == this->CapKeys.end())
		{
			reason = "Protocol version not specified";
		}
		else
		{
			proto_version = atoi(CapKeys.find("PROTOCOL")->second.c_str());
			if (proto_version < MinCompatProtocol)
			{
				reason = "Server is using protocol version " + ConvToStr(proto_version) +
					" which is too old to link with this server (version " + ConvToStr(ProtocolVersion)
					+ (ProtocolVersion != MinCompatProtocol ? ", links with " + ConvToStr(MinCompatProtocol) + " and above)" : ")");
			}
		}

		if(this->CapKeys.find("PREFIX") != this->CapKeys.end() && this->CapKeys.find("PREFIX")->second != this->ServerInstance->Modes->BuildPrefixes())
			reason = "One or more of the prefixes on the remote server are invalid on this server.";

		if(this->CapKeys.find("CHANMODES") != this->CapKeys.end() && this->CapKeys.find("CHANMODES")->second != this->ServerInstance->Modes->GiveModeList(MASK_CHANNEL))
			reason = "One or more of the channel modes on the remote server are invalid on this server.";

		if(this->CapKeys.find("USERMODES") != this->CapKeys.end() && this->CapKeys.find("USERMODES")->second != this->ServerInstance->Modes->GiveModeList(MASK_USER))
			reason = "One or more of the user modes on the remote server are invalid on this server.";


		/* Challenge response, store their challenge for our password */
		std::map<std::string,std::string>::iterator n = this->CapKeys.find("CHALLENGE");
		if (Utils->ChallengeResponse && (n != this->CapKeys.end()) && (ServerInstance->Modules->Find("m_sha256.so")))
		{
			/* Challenge-response is on now */
			this->SetTheirChallenge(n->second);
			if (!this->GetTheirChallenge().empty() && (this->LinkState == CONNECTING))
			{
				this->SendCapabilities(2);
				this->WriteLine(std::string("SERVER ")+this->ServerInstance->Config->ServerName+" "+this->MakePass(OutboundPass, this->GetTheirChallenge())+" 0 "+
						ServerInstance->Config->GetSID()+" :"+this->ServerInstance->Config->ServerDesc);
			}
		}
		else
		{
			/* They didnt specify a challenge or we don't have m_sha256.so, we use plaintext */
			if (this->LinkState == CONNECTING)
			{
				this->SendCapabilities(2);
				this->WriteLine(std::string("SERVER ")+this->ServerInstance->Config->ServerName+" "+OutboundPass+" 0 "+ServerInstance->Config->GetSID()+" :"+this->ServerInstance->Config->ServerDesc);
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
		if (!this->ModuleList.length())
		{
			this->ModuleList.append(params[1]);
		}
		else
		{
			this->ModuleList.append(",");
			this->ModuleList.append(params[1]);
		}
	}
	else if ((params[0] == "MODSUPPORT") && (params.size() == 2))
	{
		if (!this->OptModuleList.length())
		{
			this->OptModuleList.append(params[1]);
		}
		else
		{
			this->OptModuleList.append(",");
			this->OptModuleList.append(params[1]);
		}
	}
	else if ((params[0] == "CAPABILITIES") && (params.size() == 2))
	{
		irc::tokenstream capabs(params[1]);
		std::string item;
		bool more = true;
		while ((more = capabs.GetToken(item)))
		{
			/* Process each key/value pair */
			std::string::size_type equals = item.rfind('=');
			if (equals != std::string::npos)
			{
				std::string var = item.substr(0, equals);
				std::string value = item.substr(equals+1, item.length());
				CapKeys[var] = value;
			}
		}
	}
	return true;
}

