/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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

#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/main.h"

/* $ModDep: m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */


std::string TreeSocket::MyCapabilities()
{
	std::vector<std::string> modlist = this->ServerInstance->Modules->GetAllModuleNames(VF_COMMON);
	std::string capabilities;
	sort(modlist.begin(),modlist.end());
	for (std::vector<std::string>::const_iterator i = modlist.begin(); i != modlist.end(); ++i)
	{
		if (i != modlist.begin())
			capabilities = capabilities + ",";
		capabilities = capabilities + *i;
	}
	return capabilities;
}

void TreeSocket::SendCapabilities()
{
	if (sentcapab)
		return;

	sentcapab = true;
	irc::commasepstream modlist(MyCapabilities());
	this->WriteLine("CAPAB START");

	/* Send module names, split at 509 length */
	std::string item;
	std::string line = "CAPAB MODULES ";
	while (modlist.GetToken(item))
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

	int ip6 = 0;
	int ip6support = 0;
#ifdef IPV6
	ip6 = 1;
#endif
#ifdef SUPPORT_IP6LINKS
	ip6support = 1;
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
			" IP6SUPPORT="+ConvToStr(ip6support)+
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

bool TreeSocket::Capab(const std::deque<std::string> &params)
{
	if (params.size() < 1)
	{
		this->SendError("Invalid number of parameters for CAPAB - Mismatched version");
		return false;
	}
	if (params[0] == "START")
	{
		this->ModuleList.clear();
		this->CapKeys.clear();
	}
	else if (params[0] == "END")
	{
		std::string reason;
		int ip6support = 0;
#ifdef SUPPORT_IP6LINKS
		ip6support = 1;
#endif
		/* Compare ModuleList and check CapKeys...
		 * Maybe this could be tidier? -- Brain
		 */
		if ((this->ModuleList != this->MyCapabilities()) && (this->ModuleList.length()))
		{
			std::string diffIneed = ListDifference(this->ModuleList, this->MyCapabilities());
			std::string diffUneed = ListDifference(this->MyCapabilities(), this->ModuleList);
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
		if (((this->CapKeys.find("IP6SUPPORT") == this->CapKeys.end()) && (ip6support)) || ((this->CapKeys.find("IP6SUPPORT") != this->CapKeys.end()) && (this->CapKeys.find("IP6SUPPORT")->second != ConvToStr(ip6support))))
			reason = "We don't both support linking to IPV6 servers";
		if (((this->CapKeys.find("IP6NATIVE") != this->CapKeys.end()) && (this->CapKeys.find("IP6NATIVE")->second == "1")) && (!ip6support))
			reason = "The remote server is IPV6 native, and we don't support linking to IPV6 servers";
		if (this->CapKeys.find("PROTOCOL") == this->CapKeys.end())
		{
			reason = "Protocol version not specified";
		}
		else
		{
			int otherProto = atoi(CapKeys.find("PROTOCOL")->second.c_str());
			if (otherProto < MinCompatProtocol)
			{
				reason = "Server is using protocol version " + ConvToStr(otherProto) +
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
				this->SendCapabilities();
				this->WriteLine(std::string("SERVER ")+this->ServerInstance->Config->ServerName+" "+this->MakePass(OutboundPass, this->GetTheirChallenge())+" 0 "+
						ServerInstance->Config->GetSID()+" :"+this->ServerInstance->Config->ServerDesc);
			}
		}
		else
		{
			/* They didnt specify a challenge or we don't have m_sha256.so, we use plaintext */
			if (this->LinkState == CONNECTING)
			{
				this->SendCapabilities();
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

