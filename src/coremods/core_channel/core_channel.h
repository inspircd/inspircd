/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
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
#include "modules/exemption.h"

namespace Topic
{
	void ShowTopic(LocalUser* user, Channel* chan);
}

namespace Invite
{
	class APIImpl;
}

/** Handle /INVITE.
 */
class CommandInvite : public Command
{
	Invite::APIImpl& invapi;

 public:
	/** Constructor for invite.
	 */
	CommandInvite(Module* parent, Invite::APIImpl& invapiimpl);

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User*user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);
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
	CmdResult HandleLocal(const std::vector<std::string>& parameters, LocalUser* user);
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
	CmdResult HandleLocal(const std::vector<std::string>& parameters, LocalUser* user);
};

/** Handle /NAMES.
 */
class CommandNames : public SplitCommand
{
	ChanModeReference secretmode;
	ChanModeReference privatemode;
	UserModeReference invisiblemode;

 public:
	/** Constructor for names.
	 */
	CommandNames(Module* parent);

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult HandleLocal(const std::vector<std::string>& parameters, LocalUser* user);

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
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);
};
