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
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "wildcard.h"
#include "xline.h"
#include "transport.h"
#include "socketengine.h"

#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/link.h"
#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/resolvers.h"
#include "m_spanningtree/handshaketimer.h"

/* $ModDep: m_spanningtree/timesynctimer.h m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h */

/*
 * Some server somewhere in the network introducing another server.
 *	-- w
 */
bool TreeSocket::RemoteServer(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 5)
	{
		SendError("Protocol error - Missing SID");
		return false;
	}

	std::string servername = params[0];
	std::string password = params[1];
	// hopcount is not used for a remote server, we calculate this ourselves
	std::string sid = params[3];
	std::string description = params[4];
	TreeServer* ParentOfThis = Utils->FindServer(prefix);

	if (!ParentOfThis)
	{
		this->SendError("Protocol error - Introduced remote server from unknown server "+ParentOfThis->GetName());
		return false;
	}
	if (!this->Instance->IsSID(sid))
	{
		this->SendError("Invalid format server ID: "+sid+"!");
		return false;
	}
	TreeServer* CheckDupe = Utils->FindServer(servername);
	if (CheckDupe)
	{
		this->SendError("Server "+CheckDupe->GetName()+" already exists!");
		this->Instance->SNO->WriteToSnoMask('l',"Server \2"+CheckDupe->GetName()+"\2 being introduced from \2" + ParentOfThis->GetName() + "\2 denied, already exists. Closing link with " + ParentOfThis->GetName());
		return false;
	}

	Link* lnk = Utils->FindLink(servername);

	TreeServer *Node = new TreeServer(this->Utils, this->Instance, servername, description, sid, ParentOfThis,NULL, lnk ? lnk->Hidden : false);

	if (Node->DuplicateID())
	{
		this->SendError("Server ID "+servername+" already exists on the network!");
		this->Instance->SNO->WriteToSnoMask('l',"Server \2"+servername+"\2 being introduced from \2" + ParentOfThis->GetName() + "\2 denied, server ID already exists on the network. Closing link with " + ParentOfThis->GetName());
		return false;
	}

	ParentOfThis->AddChild(Node);
	params[4] = ":" + params[4];
	Utils->DoOneToAllButSender(prefix,"SERVER",params,prefix);
	this->Instance->SNO->WriteToSnoMask('l',"Server \002"+ParentOfThis->GetName()+"\002 introduced server \002"+servername+"\002 ("+description+")");
	return true;
}


/*
 * This is used after the other side of a connection has accepted our credentials.
 * They are then introducing themselves to us, BEFORE either of us burst. -- w
 */
bool TreeSocket::Outbound_Reply_Server(std::deque<std::string> &params)
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

	this->InboundServerName = sname;
	this->InboundDescription = description;
	this->InboundSID = sid;

	if (!sentcapab)
		this->SendCapabilities();

	if (hops)
	{
		this->SendError("Server too far away for authentication");
		this->Instance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, server is too far away for authentication");
		return false;
	}

	if (!this->Instance->IsSID(sid))
	{
		this->SendError("Invalid format server ID: "+sid+"!");
		return false;
	}

	for (std::vector<Link>::iterator x = Utils->LinkBlocks.begin(); x < Utils->LinkBlocks.end(); x++)
	{
		if ((x->Name == servername) && ((ComparePass(this->MakePass(x->RecvPass,this->GetOurChallenge()),password)) || (x->RecvPass == password && (this->GetTheirChallenge().empty()))))
		{
			TreeServer* CheckDupe = Utils->FindServer(sname);
			if (CheckDupe)
			{
				this->SendError("Server "+sname+" already exists on server "+CheckDupe->GetParent()->GetName()+"!");
				this->Instance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, already exists on server "+CheckDupe->GetParent()->GetName());
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

			TreeServer *Node = new TreeServer(this->Utils, this->Instance, sname, description, sid, Utils->TreeRoot, this, x->Hidden);

			if (Node->DuplicateID())
			{
				this->SendError("Server ID "+sid+" already exists on the network!");
				this->Instance->SNO->WriteToSnoMask('l',"Server \2"+assign(servername)+"\2 being introduced denied, server ID already exists on the network. Closing link.");
				return false;
			}

			Utils->TreeRoot->AddChild(Node);
			params[4] = ":" + params[4];
			Utils->DoOneToAllButSender(Instance->Config->GetSID(),"SERVER",params,sname);
			Node->bursting = true;
			this->DoBurst(Node);
			return true;
		}
	}
	this->SendError("Invalid credentials");
	this->Instance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, invalid link credentials");
	return false;
}

/*
 * Someone else is attempting to connect to us if this is called. Validate their credentials etc.
 *		-- w
 */
bool TreeSocket::Inbound_Server(std::deque<std::string> &params)
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

	this->InboundServerName = sname;
	this->InboundDescription = description;
	this->InboundSID = sid;

	if (!sentcapab)
		this->SendCapabilities();

	if (hops)
	{
		this->SendError("Server too far away for authentication");
		this->Instance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, server is too far away for authentication");
		return false;
	}

	if (!this->Instance->IsSID(sid))
	{
		this->SendError("Invalid format server ID: "+sid+"!");
		return false;
	}

	for (std::vector<Link>::iterator x = Utils->LinkBlocks.begin(); x < Utils->LinkBlocks.end(); x++)
	{
		if ((x->Name == servername) && ((ComparePass(this->MakePass(x->RecvPass,this->GetOurChallenge()),password) || x->RecvPass == password && (this->GetTheirChallenge().empty()))))
		{
			/* Check for fully initialized instances of the server by id */
			Instance->Logs->Log("m_spanningtree",DEBUG,"Looking for dupe SID %s", sid.c_str());
			TreeServer* CheckDupeSID = Utils->FindServerID(sid);
			if (CheckDupeSID)
			{
				this->SendError("Server ID "+CheckDupeSID->GetID()+" already exists on server "+CheckDupeSID->GetName()+"!");
				this->Instance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, server ID '"+CheckDupeSID->GetID()+
						"' already exists on server "+CheckDupeSID->GetName());
				return false;
			}
			/* Now check for fully initialized instances of the server by name */
			TreeServer* CheckDupe = Utils->FindServer(sname);
			if (CheckDupe)
			{
				this->SendError("Server "+sname+" already exists on server "+CheckDupe->GetParent()->GetName()+"!");
				this->Instance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, already exists on server "+CheckDupe->GetParent()->GetName());
				return false;
			}
			this->Instance->SNO->WriteToSnoMask('l',"Verified incoming server connection from \002"+sname+"\002["+(x->HiddenFromStats ? "<hidden>" : this->GetIP())+"] ("+description+")");
			if (this->Hook)
			{
				std::string name = BufferedSocketNameRequest((Module*)Utils->Creator, this->Hook).Send();
				this->Instance->SNO->WriteToSnoMask('l',"Connection from \2"+sname+"\2["+(x->HiddenFromStats ? "<hidden>" : this->GetIP())+"] using transport \2"+name+"\2");
			}

			// this is good. Send our details: Our server name and description and hopcount of 0,
			// along with the sendpass from this block.
			this->SendCapabilities();
			this->WriteLine(std::string("SERVER ")+this->Instance->Config->ServerName+" "+this->MakePass(x->SendPass, this->GetTheirChallenge())+" 0 "+Instance->Config->GetSID()+" :"+this->Instance->Config->ServerDesc);
			// move to the next state, we are now waiting for THEM.
			this->LinkState = WAIT_AUTH_2;
			return true;
		}
	}
	this->SendError("Invalid credentials");
	this->Instance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, invalid link credentials");
	return false;
}

