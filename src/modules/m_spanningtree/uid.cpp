/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2019, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
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

CmdResult CommandUID::HandleServer(TreeServer* remoteserver, CommandBase::Params& params)
{
	/**
	 *     0    1           2    3    4     5?     6(5)    7(6)      8(7)   9(8)   10(9)      (n-1)
	 * UID uuid nickchanged nick host dhost [user] duser ip.string signon +modes (modepara) :real
	 *
	 * The `duser` field was introduced in the 1206 (v4) protocol.
	 */
	size_t offset = params[9][0] == '+' ? 1 : 0;
	time_t nickchanged = ServerCommand::ExtractTS(params[1]);
	time_t signon = ServerCommand::ExtractTS(params[7+offset]);
	const std::string& modestr = params[8+offset];

	// Check if the length of the uuid is correct and confirm the sid portion of the uuid matches the sid of the server introducing the user
	if (params[0].length() != UIDGenerator::UUID_LENGTH || params[0].compare(0, 3, remoteserver->GetId()))
		throw ProtocolException("Bogus UUID");
	// Sanity check on mode string: must begin with '+'
	if (modestr[0] != '+')
		throw ProtocolException("Invalid mode string");

	// See if there is a nick collision
	auto* collideswith = ServerInstance->Users.FindNick(params[2]);
	if (collideswith && !collideswith->IsFullyConnected())
	{
		// User that the incoming user is colliding with is not fully connected, we force nick change the
		// partially connected user to their uuid and tell them what happened
		LocalUser* const localuser = static_cast<LocalUser*>(collideswith);
		localuser->OverruleNick();
	}
	else if (collideswith)
	{
		// The user on this side is fully connected, handle the collision
		bool they_change = SpanningTreeUtilities::DoCollision(collideswith, remoteserver, nickchanged, params[5], params[6+offset], params[0], "UID");
		if (they_change)
		{
			// The client being introduced needs to change nick to uuid, change the nick in the message before
			// processing/forwarding it. Also change the nick TS to CommandSave::SavedTimestamp.
			nickchanged = CommandSave::SavedTimestamp;
			params[1] = ConvToStr(CommandSave::SavedTimestamp);
			params[2] = params[0];
		}
	}

	irc::sockets::sockaddrs sa(false);
	if (!sa.from(params[6+offset]))
		throw ProtocolException("Invalid IP address or UNIX socket path");

	/* For remote users, we pass the UUID they sent to the constructor.
	 * If the UUID already exists User::User() throws an exception which causes this connection to be closed.
	 */
	auto* _new = new SpanningTree::RemoteUser(params[0], remoteserver);
	ServerInstance->Users.clientlist[params[2]] = _new;
	_new->nick = params[2];
	_new->ChangeRealHost(params[3], false);
	_new->ChangeDisplayedHost(params[4]);
	_new->ChangeRealUser(params[5], false);
	_new->ChangeDisplayedUser(params[5+offset]);
	_new->ChangeRemoteAddress(sa);
	_new->ChangeRealName(params.back());
	_new->connected = User::CONN_FULL;
	_new->signon = signon;
	_new->nickchanged = nickchanged;

	size_t paramptr = 9 + offset;
	for (const auto& modechr : modestr)
	{
		// Accept more '+' chars, for now
		if (modechr == '+')
			continue;

		/* For each mode that's set, find the mode handler and set it on the new user */
		ModeHandler* mh = ServerInstance->Modes.FindMode(modechr, MODETYPE_USER);
		if (!mh)
			throw ProtocolException("Unrecognised mode '" + std::string(1, modechr) + "'");

		if (mh->NeedsParam(true))
		{
			if (paramptr >= params.size() - 1)
				throw ProtocolException("Out of parameters while processing modes");

			/* IMPORTANT NOTE:
			 * All modes are assumed to succeed here as they are being set by a remote server.
			 * Modes CANNOT FAIL here. If they DO fail, then the failure is ignored. This is important
			 * to note as all but one modules currently cannot ever fail in this situation, except for
			 * user mode +k which specifically works this way to prevent the mode being set ANYWHERE
			 * but here, at client introduction. You may safely assume this behaviour is standard and
			 * will not change in future versions if you want to make use of this protective behaviour
			 * yourself.
			 */
			Modes::Change modechange(mh, true, params[paramptr++]);
			mh->OnModeChange(_new, _new, nullptr, modechange);
		}
		else
		{
			Modes::Change modechange(mh, true, "");
			mh->OnModeChange(_new, _new, nullptr, modechange);
		}
		_new->SetMode(mh, true);
	}

	ServerInstance->Users.AddClone(_new);
	remoteserver->UserCount++;

	bool dosend = true;

	if ((Utils->quiet_bursts && remoteserver->IsBehindBursting()) || _new->server->IsSilentService())
		dosend = false;

	if (dosend)
	{
		ServerInstance->SNO.WriteToSnoMask('C', "Client connecting at {}: {} ({}) [{}\x0F]", remoteserver->GetName(),
			_new->GetRealMask(), _new->GetAddress(), _new->GetRealName());
	}

	FOREACH_MOD(OnPostConnect, (_new));

	return CmdResult::SUCCESS;
}

CmdResult CommandFHost::HandleRemote(RemoteUser* src, Params& params)
{
	if (params[0] != "*")
		src->ChangeDisplayedHost(params[0]);

	if (params[1] != "*")
		src->ChangeRealHost(params[1], false);

	return CmdResult::SUCCESS;
}

CmdResult CommandFIdent::HandleRemote(RemoteUser* src, Params& params)
{
	if (params[0] != "*")
		src->ChangeDisplayedUser(params[0]);

	if (params[1] != "*")
		src->ChangeRealUser(params[1], false);

	return CmdResult::SUCCESS;
}

CmdResult CommandFName::HandleRemote(RemoteUser* src, Params& params)
{
	src->ChangeRealName(params[0]);
	return CmdResult::SUCCESS;
}

CommandUID::Builder::Builder(User* user, bool real_user)
	: CmdBuilder(TreeServer::Get(user), "UID")
{
	push(user->uuid);
	push_int(user->nickchanged);
	push(user->nick);
	push(user->GetRealHost());
	push(user->GetDisplayedHost());
	if (real_user)
		push(user->GetRealUser());
	push(user->GetDisplayedUser());
	push(user->GetAddress());
	push_int(user->signon);
	push(user->GetModeLetters(true));
	push_last(user->GetRealName());
}
