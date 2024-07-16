/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014-2015, 2018 Attila Molnar <attilamolnar@hush.com>
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

enum
{
	// From RFC 1459.
	RPL_YOUAREOPER = 381,
	ERR_NOOPERHOST = 491,
};

namespace DieRestart
{
	/** Send an ERROR to partially connected users and a NOTICE to fully connected users
	 * @param message Message to send
	 */
	void SendError(const std::string& message);
}

class CommandDie final
	: public Command
{
public:
	CommandDie(Module* parent);
	CmdResult Handle(User* user, const Params& parameters) override;
};

class CommandKill final
	: public Command
{
	std::string lastuuid;
	std::string killreason;
	ClientProtocol::EventProvider protoev;

public:
	/** Set to a non empty string to obfuscate nicknames prepended to a KILL. */
	std::string hidenick;

	/** Set to hide kills from clients of services servers in snotices. */
	bool hideservicekills;

	CommandKill(Module* parent);
	CmdResult Handle(User* user, const Params& parameters) override;
	RouteDescriptor GetRouting(User* user, const Params& parameters) override;
	void EncodeParameter(std::string& param, unsigned int index) override;
};

class CommandOper final
	: public SplitCommand
{
public:
	CommandOper(Module* parent);
	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override;
};

class CommandRehash final
	: public Command
{
public:
	CommandRehash(Module* parent);
	CmdResult Handle(User* user, const Params& parameters) override;
};

class CommandRestart final
	: public Command
{
public:
	CommandRestart(Module* parent);
	CmdResult Handle(User* user, const Params& parameters) override;
};

class ModeUserServerNoticeMask final
	: public ModeHandler
{
private:
	/** Process a snomask modifier string, e.g. +abc-de
	 * @param user The target user
	 * @param input A sequence of notice mask characters
	 * @return The cleaned mode sequence which can be output,
	 * e.g. in the above example if masks c and e are not
	 * valid, this function will return +ab-d
	 */
	std::string ProcessNoticeMasks(User* user, const std::string& input);

public:
	ModeUserServerNoticeMask(Module* Creator);
	bool OnModeChange(User* source, User* dest, Channel* channel, Modes::Change& change) override;

	/** Create a displayable mode string of the snomasks set on a given user
	 * @param user The user whose notice masks to format
	 * @return The notice mask character sequence
	 */
	std::string GetUserParameter(const User* user) const override;
};

class ModeUserOperator final
	: public SimpleUserMode
{
public:
	ModeUserOperator(Module* Creator);
	bool OnModeChange(User* source, User* dest, Channel* channel, Modes::Change& change) override;
};
