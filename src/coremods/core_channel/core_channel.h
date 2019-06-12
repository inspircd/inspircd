/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
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


#pragma once

#include "inspircd.h"
#include "listmode.h"
#include "modules/exemption.h"

namespace Topic
{
	void ShowTopic(LocalUser* user, Channel* chan);
}

namespace Invite
{
	class APIImpl;

	/** Used to indicate who we announce invites to on a channel. */
	enum AnnounceState
	{
		/** Don't send invite announcements. */
		ANNOUNCE_NONE,

		/** Send invite announcements to all users. */
		ANNOUNCE_ALL,

		/** Send invite announcements to channel operators and higher. */
		ANNOUNCE_OPS,

		/** Send invite announcements to channel half-operators (if available) and higher. */
		ANNOUNCE_DYNAMIC
	};
}

/** Handle /INVITE.
 */
class CommandInvite : public Command
{
	Invite::APIImpl& invapi;

 public:
	Invite::AnnounceState announceinvites;

	/** Constructor for invite.
	 */
	CommandInvite(Module* parent, Invite::APIImpl& invapiimpl);

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;
	RouteDescriptor GetRouting(User* user, const Params& parameters) CXX11_OVERRIDE;
};

/** Handle /JOIN.
 */
class CommandJoin : public SplitCommand
{
 public:
	/** Constructor for join.
	 */
	CommandJoin(Module* parent);

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult HandleLocal(LocalUser* user, const Params& parameters) CXX11_OVERRIDE;
};

/** Handle /TOPIC.
 */
class CommandTopic : public SplitCommand
{
	CheckExemption::EventProvider exemptionprov;
	ChanModeReference secretmode;
	ChanModeReference topiclockmode;

 public:
	/** Constructor for topic.
	 */
	CommandTopic(Module* parent);

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult HandleLocal(LocalUser* user, const Params& parameters) CXX11_OVERRIDE;
};

/** Handle /NAMES.
 */
class CommandNames : public SplitCommand
{
 private:
	ChanModeReference secretmode;
	ChanModeReference privatemode;
	UserModeReference invisiblemode;
	Events::ModuleEventProvider namesevprov;

 public:
	/** Constructor for names.
	 */
	CommandNames(Module* parent);

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult HandleLocal(LocalUser* user, const Params& parameters) CXX11_OVERRIDE;

	/** Spool the NAMES list for a given channel to the given user
	 * @param user User to spool the NAMES list to
	 * @param chan Channel whose nicklist to send
	 * @param show_invisible True to show invisible (+i) members to the user, false to omit them from the list
	 */
	void SendNames(LocalUser* user, Channel* chan, bool show_invisible);
};

/** Handle /KICK.
 */
class CommandKick : public Command
{
 public:
	/** Constructor for kick.
	 */
	CommandKick(Module* parent);

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;
	RouteDescriptor GetRouting(User* user, const Params& parameters) CXX11_OVERRIDE;
};

/** Channel mode +b
 */
class ModeChannelBan : public ListModeBase
{
 public:
	ModeChannelBan(Module* Creator)
		: ListModeBase(Creator, "ban", 'b', "End of channel ban list", 367, 368, true)
	{
		syntax = "<mask>";
	}
};

/** Channel mode +k
 */
class ModeChannelKey : public ParamMode<ModeChannelKey, LocalStringExt>
{
 public:
	static const std::string::size_type maxkeylen;
	ModeChannelKey(Module* Creator);
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string& parameter, bool adding) CXX11_OVERRIDE;
	void SerializeParam(Channel* chan, const std::string* key, std::string& out)	;
	ModeAction OnSet(User* source, Channel* chan, std::string& param) CXX11_OVERRIDE;
	bool IsParameterSecret() CXX11_OVERRIDE;
};

/** Channel mode +l
 */
class ModeChannelLimit : public ParamMode<ModeChannelLimit, LocalIntExt>
{
 public:
	size_t minlimit;
	ModeChannelLimit(Module* Creator);
	bool ResolveModeConflict(std::string& their_param, const std::string& our_param, Channel* channel) CXX11_OVERRIDE;
	void SerializeParam(Channel* chan, intptr_t n, std::string& out);
	ModeAction OnSet(User* source, Channel* channel, std::string& parameter) CXX11_OVERRIDE;
};

/** Channel mode +o
 */
class ModeChannelOp : public PrefixMode
{
 public:
	ModeChannelOp(Module* Creator)
		: PrefixMode(Creator, "op", 'o', OP_VALUE, '@')
	{
		ranktoset = ranktounset = OP_VALUE;
	}
};

/** Channel mode +v
 */
class ModeChannelVoice : public PrefixMode
{
 public:
	ModeChannelVoice(Module* Creator)
		: PrefixMode(Creator, "voice", 'v', VOICE_VALUE, '+')
	{
		selfremove = false;
		ranktoset = ranktounset = HALFOP_VALUE;
	}
};
