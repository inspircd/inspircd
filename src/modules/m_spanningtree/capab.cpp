/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
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
	std::vector<std::string> modlist = this->Instance->Modules->GetAllModuleNames(VF_COMMON);
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

void TreeSocket::SendCapabilities()
{
	if (sentcapab)
		return;

	sentcapab = true;
	irc::commasepstream modulelist(MyCapabilities());
	this->WriteLine("CAPAB START");

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
	if (Utils->ChallengeResponse && (Instance->Modules->Find("m_sha256.so")))
	{
		this->SetOurChallenge(RandString(20));
		extra = " CHALLENGE=" + this->GetOurChallenge();
	}

	this->WriteLine("CAPAB CAPABILITIES " /* Preprocessor does this one. */
			":NICKMAX="+ConvToStr(NICKMAX)+
			" HALFOP="+ConvToStr(this->Instance->Config->AllowHalfop)+
			" CHANMAX="+ConvToStr(CHANMAX)+
			" MAXMODES="+ConvToStr(MAXMODES)+
			" IDENTMAX="+ConvToStr(IDENTMAX)+
			" MAXQUIT="+ConvToStr(MAXQUIT)+
			" MAXTOPIC="+ConvToStr(MAXTOPIC)+
			" MAXKICK="+ConvToStr(MAXKICK)+
			" MAXGECOS="+ConvToStr(MAXGECOS)+
			" MAXAWAY="+ConvToStr(MAXAWAY)+
			" IP6NATIVE="+ConvToStr(ip6)+
			" IP6SUPPORT="+ConvToStr(ip6support)+
			" PROTOCOL="+ConvToStr(ProtocolVersion)+extra+
			" PREFIX="+Instance->Modes->BuildPrefixes()+
			" CHANMODES="+Instance->Modes->ChanModes()+
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
			std::string diff = ListDifference(this->ModuleList, this->MyCapabilities());
			if (!diff.length())
			{
				diff = "your server:" + ListDifference(this->MyCapabilities(), this->ModuleList);
			}
			else
			{
				diff = "this server:" + diff;
			}
			if (diff.length() == 12)
				reason = "Module list in CAPAB is not alphabetically ordered, cannot compare lists.";
			else
				reason = "Modules loaded on these servers are not correctly matched, these modules are not loaded on " + diff;
		}

		cap_validation valid_capab[] = { 
			{"Maximum nickname lengths differ or remote nickname length not specified", "NICKMAX", NICKMAX},
			{"Maximum ident lengths differ or remote ident length not specified", "IDENTMAX", IDENTMAX},
			{"Maximum channel lengths differ or remote channel length not specified", "CHANMAX", CHANMAX},
			{"Maximum modes per line differ or remote modes per line not specified", "MAXMODES", MAXMODES},
			{"Maximum quit lengths differ or remote quit length not specified", "MAXQUIT", MAXQUIT},
			{"Maximum topic lengths differ or remote topic length not specified", "MAXTOPIC", MAXTOPIC},
			{"Maximum kick lengths differ or remote kick length not specified", "MAXKICK", MAXKICK},
			{"Maximum GECOS (fullname) lengths differ or remote GECOS length not specified", "MAXGECOS", MAXGECOS},
			{"Maximum awaymessage lengths differ or remote awaymessage length not specified", "MAXAWAY", MAXAWAY},
			{"", "", 0}
		};

		if (((this->CapKeys.find("IP6SUPPORT") == this->CapKeys.end()) && (ip6support)) || ((this->CapKeys.find("IP6SUPPORT") != this->CapKeys.end()) && (this->CapKeys.find("IP6SUPPORT")->second != ConvToStr(ip6support))))
			reason = "We don't both support linking to IPV6 servers";
		if (((this->CapKeys.find("IP6NATIVE") != this->CapKeys.end()) && (this->CapKeys.find("IP6NATIVE")->second == "1")) && (!ip6support))
			reason = "The remote server is IPV6 native, and we don't support linking to IPV6 servers";
		if (((this->CapKeys.find("PROTOCOL") == this->CapKeys.end()) || ((this->CapKeys.find("PROTOCOL") != this->CapKeys.end()) && (this->CapKeys.find("PROTOCOL")->second != ConvToStr(ProtocolVersion)))))
		{
			if (this->CapKeys.find("PROTOCOL") != this->CapKeys.end())
				reason = "Mismatched protocol versions "+this->CapKeys.find("PROTOCOL")->second+" and "+ConvToStr(ProtocolVersion);
			else
				reason = "Protocol version not specified";
		}

		if(this->CapKeys.find("PREFIX") != this->CapKeys.end() && this->CapKeys.find("PREFIX")->second != this->Instance->Modes->BuildPrefixes())
			reason = "One or more of the prefixes on the remote server are invalid on this server.";

		if (((this->CapKeys.find("HALFOP") == this->CapKeys.end()) && (Instance->Config->AllowHalfop)) || ((this->CapKeys.find("HALFOP") != this->CapKeys.end()) && (this->CapKeys.find("HALFOP")->second != ConvToStr(Instance->Config->AllowHalfop))))
			reason = "We don't both have halfop support enabled/disabled identically";

		for (int x = 0; valid_capab[x].size; ++x)
		{
			if (((this->CapKeys.find(valid_capab[x].key) == this->CapKeys.end()) ||	((this->CapKeys.find(valid_capab[x].key) != this->CapKeys.end()) &&
						 (this->CapKeys.find(valid_capab[x].key)->second != ConvToStr(valid_capab[x].size)))))
				reason = valid_capab[x].reason;
		}
	
		/* Challenge response, store their challenge for our password */
		std::map<std::string,std::string>::iterator n = this->CapKeys.find("CHALLENGE");
		if (Utils->ChallengeResponse && (n != this->CapKeys.end()) && (Instance->Modules->Find("m_sha256.so")))
		{
			/* Challenge-response is on now */
			this->SetTheirChallenge(n->second);
			if (!this->GetTheirChallenge().empty() && (this->LinkState == CONNECTING))
			{
				this->SendCapabilities();
				this->WriteLine(std::string("SERVER ")+this->Instance->Config->ServerName+" "+this->MakePass(OutboundPass, this->GetTheirChallenge())+" 0 "+
						Instance->Config->GetSID()+" :"+this->Instance->Config->ServerDesc);
			}
		}
		else
		{
			/* They didnt specify a challenge or we don't have m_sha256.so, we use plaintext */
			if (this->LinkState == CONNECTING)
			{
				this->SendCapabilities();
				this->WriteLine(std::string("SERVER ")+this->Instance->Config->ServerName+" "+OutboundPass+" 0 "+Instance->Config->GetSID()+" :"+this->Instance->Config->ServerDesc);
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

