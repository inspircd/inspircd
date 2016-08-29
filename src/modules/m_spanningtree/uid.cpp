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
#include "treeserver.h"
#include "remoteuser.h"

CmdResult CommandUID::HandleServer(TreeServer* remoteserver, std::vector<std::string>& params)
{
	/**
	 *      0    1    2    3    4    5        6        7     8        9       (n-1)
	 * UID uuid age nick host dhost ident ip.string signon +modes (modepara) :gecos
	 */
	time_t age_t = ServerCommand::ExtractTS(params[1]);
	time_t signon = ServerCommand::ExtractTS(params[7]);
	std::string empty;
	const std::string& modestr = params[8];

	// Check if the length of the uuid is correct and confirm the sid portion of the uuid matches the sid of the server introducing the user
	if (params[0].length() != UIDGenerator::UUID_LENGTH || params[0].compare(0, 3, remoteserver->GetID()))
		throw ProtocolException("Bogus UUID");
	// Sanity check on mode string: must begin with '+'
	if (modestr[0] != '+')
		throw ProtocolException("Invalid mode string");

	// See if there is a nick collision
	User* collideswith = ServerInstance->FindNickOnly(params[2]);
	if ((collideswith) && (collideswith->registered != REG_ALL))
	{
		// User that the incoming user is colliding with is not fully registered, we force nick change the
		// unregistered user to their uuid and tell them what happened
		collideswith->WriteFrom(collideswith, "NICK %s", collideswith->uuid.c_str());
		collideswith->WriteNumeric(ERR_NICKNAMEINUSE, collideswith->nick, "Nickname overruled.");

		// Clear the bit before calling User::ChangeNick() to make it NOT run the OnUserPostNick() hook
		collideswith->registered &= ~REG_NICK;
		collideswith->ChangeNick(collideswith->uuid);
	}
	else if (collideswith)
	{
		// The user on this side is registered, handle the collision
		bool they_change = Utils->DoCollision(collideswith, remoteserver, age_t, params[5], params[6], params[0], "UID");
		if (they_change)
		{
			// The client being introduced needs to change nick to uuid, change the nick in the message before
			// processing/forwarding it. Also change the nick TS to CommandSave::SavedTimestamp.
			age_t = CommandSave::SavedTimestamp;
			params[1] = ConvToStr(CommandSave::SavedTimestamp);
			params[2] = params[0];
		}
	}

	/* For remote users, we pass the UUID they sent to the constructor.
	 * If the UUID already exists User::User() throws an exception which causes this connection to be closed.
	 */
	RemoteUser* _new = new SpanningTree::RemoteUser(params[0], remoteserver);
	ServerInstance->Users->clientlist[params[2]] = _new;
	_new->nick = params[2];
	_new->host = params[3];
	_new->dhost = params[4];
	_new->ident = params[5];
	_new->fullname = params.back();
	_new->registered = REG_ALL;
	_new->signon = signon;
	_new->age = age_t;

	unsigned int paramptr = 9;

	for (std::string::const_iterator v = modestr.begin(); v != modestr.end(); ++v)
	{
		// Accept more '+' chars, for now
		if (*v == '+')
			continue;

		/* For each mode thats set, find the mode handler and set it on the new user */
		ModeHandler* mh = ServerInstance->Modes->FindMode(*v, MODETYPE_USER);
		if (!mh)
			throw ProtocolException("Unrecognised mode '" + std::string(1, *v) + "'");

		if (mh->NeedsParam(true))
		{
			if (paramptr >= params.size() - 1)
				throw ProtocolException("Out of parameters while processing modes");
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
		_new->SetMode(mh, true);
	}

	_new->SetClientIP(params[6].c_str());

	ServerInstance->Users->AddClone(_new);
	remoteserver->UserCount++;

	bool dosend = true;

	if ((Utils->quiet_bursts && remoteserver->IsBehindBursting()) || _new->server->IsSilentULine())
		dosend = false;

	if (dosend)
		ServerInstance->SNO->WriteToSnoMask('C',"Client connecting at %s: %s (%s) [%s]", remoteserver->GetName().c_str(), _new->GetFullRealHost().c_str(), _new->GetIPString().c_str(), _new->fullname.c_str());

	FOREACH_MOD(OnPostConnect, (_new));

	return CMD_SUCCESS;
}

CmdResult CommandFHost::HandleRemote(RemoteUser* src, std::vector<std::string>& params)
{
	src->ChangeDisplayedHost(params[0]);
	return CMD_SUCCESS;
}

CmdResult CommandFIdent::HandleRemote(RemoteUser* src, std::vector<std::string>& params)
{
	src->ChangeIdent(params[0]);
	return CMD_SUCCESS;
}

CmdResult CommandFName::HandleRemote(RemoteUser* src, std::vector<std::string>& params)
{
	src->ChangeName(params[0]);
	return CMD_SUCCESS;
}

CommandUID::Builder::Builder(User* user)
	: CmdBuilder(TreeServer::Get(user)->GetID(), "UID")
{
	push(user->uuid);
	push_int(user->age);
	push(user->nick);
	push(user->host);
	push(user->dhost);
	push(user->ident);
	push(user->GetIPString());
	push_int(user->signon);
	push('+').push_raw(user->FormatModes(true));
	push_last(user->fullname);
}
