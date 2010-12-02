/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
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
			this->SendError("Server "+sname+" already exists on server "+CheckDupe->GetParent()->GetName()+"!");
			ServerInstance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, already exists on server "+CheckDupe->GetParent()->GetName());
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
		params[4] = ":" + params[4];

		/* IMPORTANT: Take password/hmac hash OUT of here before we broadcast the introduction! */
		params[1] = "*";
		Utils->DoOneToAllButSender(ServerInstance->Config->GetSID(),"SERVER",params,sname);

		this->DoBurst(MyRoot);
		return true;
	}

	this->SendError("Invalid credentials (check the other server's linking snomask for more information)");
	ServerInstance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, invalid link credentials");
	return false;
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

		/* Now check for fully initialized ServerInstances of the server by name */
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

		ServerInstance->SNO->WriteToSnoMask('l',"Verified incoming server connection " + linkID + " ("+description+")");
		linkID = sname;

		// this is good. Send our details: Our server name and description and hopcount of 0,
		// along with the sendpass from this block.
		this->SendCapabilities(2);
		this->WriteLine(std::string("SERVER ")+ServerInstance->Config->ServerName+" "+this->MakePass(x->SendPass, this->GetTheirChallenge())+" 0 "+ServerInstance->Config->GetSID()+" :"+ServerInstance->Config->ServerDesc);
		// move to the next state, we are now waiting for THEM.
		MyRoot = new TreeServer(Utils, sname, description, sid, Utils->TreeRoot, this, x->Hidden);
		Utils->TreeRoot->AddChild(MyRoot);

		params[1] = "*";
		params[4] = ":" + params[4];
		Utils->DoOneToAllButSender(ServerInstance->Config->GetSID(),"SERVER",params,sname);

		this->LinkState = WAIT_AUTH_2;
		return true;
	}

	this->SendError("Invalid credentials");
	ServerInstance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, invalid link credentials");
	return false;
}

