/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
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
#include "commands.h"

#include "utils.h"
#include "link.h"
#include "treesocket.h"
#include "treeserver.h"
#include "resolvers.h"

CmdResult CommandUID::Handle(const parameterlist &params, User* serversrc)
{
	SpanningTreeUtilities* Utils = ((ModuleSpanningTree*)(Module*)creator)->Utils;
	/** Do we have enough parameters:
	 *      0    1    2    3    4    5        6        7     8        9       (n-1)
	 * UID uuid age nick host dhost ident ip.string signon +modes (modepara) :gecos
	 */
	time_t age_t = ConvToInt(params[1]);
	time_t signon = ConvToInt(params[7]);
	std::string empty;
	std::string modestr(params[8]);

	TreeServer* remoteserver = Utils->FindServer(serversrc->server);

	if (!remoteserver)
		return CMD_INVALID;
	/* Is this a valid UID, and not misrouted? */
	if (params[0].length() != 9 || params[0].substr(0,3) != serversrc->uuid)
		return CMD_INVALID;
	/* Check parameters for validity before introducing the client, discovered by dmb */
	if (!age_t)
		return CMD_INVALID;
	if (!signon)
		return CMD_INVALID;
	if (modestr[0] != '+')
		return CMD_INVALID;
	TreeSocket* sock = remoteserver->GetRoute()->GetSocket();

	/* check for collision */
	User* const collideswith = ServerInstance->FindNickOnly(params[2]);

	if ((collideswith) && (collideswith->registered != REG_ALL))
	{
		// User that the incoming user is colliding with is not fully registered, we force nick change the
		// unregistered user to their uuid and tell them what happened
		collideswith->WriteFrom(collideswith, "NICK %s", collideswith->uuid.c_str());
		collideswith->WriteNumeric(433, "%s %s :Nickname overruled.", collideswith->nick.c_str(), collideswith->nick.c_str());

		// Clear the bit before calling User::ChangeNick() to make it NOT run the OnUserPostNick() hook
		collideswith->registered &= ~REG_NICK;
		collideswith->ChangeNick(collideswith->uuid, true);
	}
	else if (collideswith)
	{
		/*
		 * Nick collision.
		 */
		int collide = sock->DoCollision(collideswith, age_t, params[5], params[6], params[0]);
		ServerInstance->Logs->Log("m_spanningtree",DEBUG,"*** Collision on %s, collide=%d", params[2].c_str(), collide);

		if (collide != 1)
		{
			/* remote client lost, make sure we change their nick for the hash too
			 *
			 * This alters the line that will be sent to other servers, which
			 * commands normally shouldn't do; hence the required const_cast.
			 */
			const_cast<parameterlist&>(params)[2] = params[0];
		}
	}

	/* IMPORTANT NOTE: For remote users, we pass the UUID in the constructor. This automatically
	 * sets it up in the UUID hash for us.
	 */
	User* _new = NULL;
	try
	{
		_new = new RemoteUser(params[0], remoteserver->GetName());
	}
	catch (...)
	{
		ServerInstance->Logs->Log("m_spanningtree", DEFAULT, "Duplicate UUID %s in client introduction", params[0].c_str());
		return CMD_INVALID;
	}
	(*(ServerInstance->Users->clientlist))[params[2]] = _new;
	_new->nick = params[2];
	_new->host = params[3];
	_new->dhost = params[4];
	_new->ident = params[5];
	_new->fullname = params[params.size() - 1];
	_new->registered = REG_ALL;
	_new->signon = signon;
	_new->age = age_t;

	/* we need to remove the + from the modestring, so we can do our stuff */
	std::string::size_type pos_after_plus = modestr.find_first_not_of('+');
	if (pos_after_plus != std::string::npos)
	modestr = modestr.substr(pos_after_plus);

	unsigned int paramptr = 9;
	for (std::string::iterator v = modestr.begin(); v != modestr.end(); v++)
	{
		/* For each mode thats set, increase counter */
		ModeHandler* mh = ServerInstance->Modes->FindMode(*v, MODETYPE_USER);

		if (mh)
		{
			if (mh->GetNumParams(true))
			{
				if (paramptr >= params.size() - 1)
					return CMD_INVALID;
				std::string mp = params[paramptr++];
				/* IMPORTANT NOTE:
				 * All modes are assumed to succeed here as they are being set by a remote server.
				 * Modes CANNOT FAIL here. If they DO fail, then the failure is ignored. This is important
				 * to note as all but one modules currently cannot ever fail in this situation, except for
				 * m_servprotect which specifically works this way to prevent the mode being set ANYWHERE
				 * but here, at client introduction. You may safely assume this behaviour is standard and
				 * will not change in future versions if you want to make use of this protective behaviour
				 * yourself.
				 */
				mh->OnModeChange(_new, _new, NULL, mp, true);
			}
			else
				mh->OnModeChange(_new, _new, NULL, empty, true);
			_new->SetMode(*v, true);
		}
	}

	/* now we've done with modes processing, put the + back for remote servers */
	if (modestr[0] != '+')
		modestr = "+" + modestr;

	_new->SetClientIP(params[6].c_str());

	ServerInstance->Users->AddGlobalClone(_new);
	remoteserver->SetUserCount(1); // increment by 1

	bool dosend = true;

	if ((Utils->quiet_bursts && remoteserver->bursting) || ServerInstance->SilentULine(_new->server))
		dosend = false;

	if (dosend)
		ServerInstance->SNO->WriteToSnoMask('C',"Client connecting at %s: %s (%s) [%s]", _new->server.c_str(), _new->GetFullRealHost().c_str(), _new->GetIPString(), _new->fullname.c_str());

	FOREACH_MOD(I_OnPostConnect,OnPostConnect(_new));

	return CMD_SUCCESS;
}

CmdResult CommandFHost::Handle(const parameterlist &params, User* src)
{
	if (IS_SERVER(src))
		return CMD_FAILURE;
	src->ChangeDisplayedHost(params[0].c_str());
	return CMD_SUCCESS;
}

CmdResult CommandFIdent::Handle(const parameterlist &params, User* src)
{
	if (IS_SERVER(src))
		return CMD_FAILURE;
	src->ChangeIdent(params[0].c_str());
	return CMD_SUCCESS;
}

CmdResult CommandFName::Handle(const parameterlist &params, User* src)
{
	if (IS_SERVER(src))
		return CMD_FAILURE;
	src->ChangeName(params[0].c_str());
	return CMD_SUCCESS;
}

