/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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
#include "socket.h"
#include "xline.h"
#include "socketengine.h"

#include "main.h"
#include "utils.h"
#include "link.h"
#include "treeserver.h"
#include "treesocket.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h m_spanningtree/link.h */

/*
 * Some server somewhere in the network introducing another server.
 *	-- w
 */
bool TreeSocket::RemoteServer(const std::string &prefix, parameterlist &params)
{
	if (params.size() < 5)
	{
		SendError("Protocol error - Not enough parameters for SERVER command");
		return false;
	}

	std::string servername = params[0];
	// password is not used for a remote server
	// hopcount is not used (ever)
	std::string sid = params[3];
	std::string description = params[4];
	TreeServer* ParentOfThis = Utils->FindServer(prefix);

	if (!ParentOfThis)
	{
		this->SendError("Protocol error - Introduced remote server from unknown server "+prefix);
		return false;
	}
	if (!ServerInstance->IsSID(sid))
	{
		this->SendError("Invalid format server ID: "+sid+"!");
		return false;
	}
	TreeServer* CheckDupe = Utils->FindServer(servername);
	if (CheckDupe)
	{
		this->SendError("Server "+servername+" already exists!");
		ServerInstance->SNO->WriteToSnoMask('L', "Server \2"+CheckDupe->GetName()+"\2 being introduced from \2" + ParentOfThis->GetName() + "\2 denied, already exists. Closing link with " + ParentOfThis->GetName());
		return false;
	}
	CheckDupe = Utils->FindServer(sid);
	if (CheckDupe)
	{
		this->SendError("Server ID "+sid+" already exists! You may want to specify the server ID for the server manually with <server:id> so they do not conflict.");
		ServerInstance->SNO->WriteToSnoMask('L', "Server \2"+servername+"\2 being introduced from \2" + ParentOfThis->GetName() + "\2 denied, server ID already exists on the network. Closing link with " + ParentOfThis->GetName());
		return false;
	}


	Link* lnk = Utils->FindLink(servername);

	TreeServer *Node = new TreeServer(Utils, servername, description, sid, ParentOfThis,NULL, lnk ? lnk->Hidden : false);

	ParentOfThis->AddChild(Node);
	params[4] = ":" + params[4];
	Utils->DoOneToAllButSender(prefix,"SERVER",params,prefix);
	ServerInstance->SNO->WriteToSnoMask('L', "Server \002"+ParentOfThis->GetName()+"\002 introduced server \002"+servername+"\002 ("+description+")");
	return true;
}


/*
 * This is used after the other side of a connection has accepted our credentials.
 * They are then introducing themselves to us, BEFORE either of us burst. -- w
 */
bool TreeSocket::Outbound_Reply_Server(parameterlist &params)
{
	if (params.size() < 5)
	{
		SendError("Protocol error - Not enough parameters for SERVER command");
		return false;
	}

	irc::string servername = params[0].c_str();
	std::string sname = params[0];
	std::string password = params[1];
	std::string sid = params[3];
	std::string description = params[4];
	int hops = atoi(params[2].c_str());

	this->SendCapabilities(2);

	if (hops)
	{
		this->SendError("Server too far away for authentication");
		ServerInstance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, server is too far away for authentication");
		return false;
	}

	if (!ServerInstance->IsSID(sid))
	{
		this->SendError("Invalid format server ID: "+sid+"!");
		return false;
	}

	for (std::vector<reference<Link> >::iterator i = Utils->LinkBlocks.begin(); i < Utils->LinkBlocks.end(); i++)
	{
		Link* x = *i;
		if (x->Name != servername && x->Name != "*") // open link allowance
			continue;

		if (!ComparePass(*x, password))
		{
			ServerInstance->SNO->WriteToSnoMask('l',"Invalid password on link: %s", x->Name.c_str());
			continue;
		}

		TreeServer* CheckDupe = Utils->FindServer(sname);
		if (CheckDupe)
		{
			std::string pname = CheckDupe->GetParent() ? CheckDupe->GetParent()->GetName() : "<ourself>";
			SendError("Server "+sname+" already exists on server "+pname+"!");
			ServerInstance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, already exists on server "+pname);
			return false;
		}
		CheckDupe = Utils->FindServer(sid);
		if (CheckDupe)
		{
			this->SendError("Server ID "+sid+" already exists on the network! You may want to specify the server ID for the server manually with <server:id> so they do not conflict.");
			ServerInstance->SNO->WriteToSnoMask('l',"Server \2"+assign(servername)+"\2 being introduced denied, server ID already exists on the network. Closing link.");
			return false;
		}

		/*
		 * They're in WAIT_AUTH_2 (having accepted our credentials).
		 * Set our state to CONNECTED (since everything's peachy so far) and send our
		 * netburst to them, which will trigger their CONNECTED state, and BURST in reply.
		 *
		 * While we're at it, create a treeserver object so we know about them.
		 *   -- w
		 */
		this->LinkState = CONNECTED;

		Utils->timeoutlist.erase(this);
		linkID = sname;

		MyRoot = new TreeServer(Utils, sname, description, sid, Utils->TreeRoot, this, x->Hidden);
		Utils->TreeRoot->AddChild(MyRoot);
		this->DoBurst(MyRoot);

		params[4] = ":" + params[4];

		/* IMPORTANT: Take password/hmac hash OUT of here before we broadcast the introduction! */
		params[1] = "*";
		Utils->DoOneToAllButSender(ServerInstance->Config->GetSID(),"SERVER",params,sname);

		return true;
	}

	this->SendError("Mismatched server name or password (check the other server's snomask output for details - e.g. umode +s +Ll)");
	ServerInstance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, invalid link credentials");
	return false;
}

bool TreeSocket::CheckDuplicate(const std::string& sname, const std::string& sid)
{
	/* Check for fully initialized instances of the server by name */
	TreeServer* CheckDupe = Utils->FindServer(sname);
	if (CheckDupe)
	{
		std::string pname = CheckDupe->GetParent() ? CheckDupe->GetParent()->GetName() : "<ourself>";
		SendError("Server "+sname+" already exists on server "+pname+"!");
		ServerInstance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, already exists on server "+pname);
		return false;
	}

	/* Check for fully initialized instances of the server by id */
	ServerInstance->Logs->Log("m_spanningtree",DEBUG,"Looking for dupe SID %s", sid.c_str());
	CheckDupe = Utils->FindServerID(sid);

	if (CheckDupe)
	{
		this->SendError("Server ID "+CheckDupe->GetID()+" already exists on server "+CheckDupe->GetName()+"! You may want to specify the server ID for the server manually with <server:id> so they do not conflict.");
		ServerInstance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, server ID '"+CheckDupe->GetID()+
				"' already exists on server "+CheckDupe->GetName());
		return false;
	}

	return true;
}

/*
 * Someone else is attempting to connect to us if this is called. Validate their credentials etc.
 *		-- w
 */
bool TreeSocket::Inbound_Server(parameterlist &params)
{
	if (params.size() < 5)
	{
		SendError("Protocol error - Missing SID");
		return false;
	}

	irc::string servername = params[0].c_str();
	std::string sname = params[0];
	std::string password = params[1];
	std::string sid = params[3];
	std::string description = params[4];
	int hops = atoi(params[2].c_str());

	this->SendCapabilities(2);

	if (hops)
	{
		this->SendError("Server too far away for authentication");
		ServerInstance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, server is too far away for authentication");
		return false;
	}

	if (!ServerInstance->IsSID(sid))
	{
		this->SendError("Invalid format server ID: "+sid+"!");
		return false;
	}

	for (std::vector<reference<Link> >::iterator i = Utils->LinkBlocks.begin(); i < Utils->LinkBlocks.end(); i++)
	{
		Link* x = *i;
		if (x->Name != servername && x->Name != "*") // open link allowance
			continue;

		if (!ComparePass(*x, password))
		{
			ServerInstance->SNO->WriteToSnoMask('l',"Invalid password on link: %s", x->Name.c_str());
			continue;
		}

		if (!CheckDuplicate(sname, sid))
			return false;

		ServerInstance->SNO->WriteToSnoMask('l',"Verified incoming server connection " + linkID + " ("+description+")");

		this->SendCapabilities(2);

		// Save these for later, so when they accept our credentials (indicated by BURST) we remember them
		this->capab->hidden = x->Hidden;
		this->capab->sid = sid;
		this->capab->description = description;
		this->capab->name = sname;

		// Send our details: Our server name and description and hopcount of 0,
		// along with the sendpass from this block.
		this->WriteLine("SERVER "+ServerInstance->Config->ServerName+" "+this->MakePass(x->SendPass, this->GetTheirChallenge())+" 0 "+ServerInstance->Config->GetSID()+" :"+ServerInstance->Config->ServerDesc);

		// move to the next state, we are now waiting for THEM.
		this->LinkState = WAIT_AUTH_2;
		return true;
	}

	this->SendError("Mismatched server name or password (check the other server's snomask output for details - e.g. umode +s +Ll)");
	ServerInstance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, invalid link credentials");
	return false;
}

