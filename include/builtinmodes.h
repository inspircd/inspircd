/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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

#include "mode.h"
#include "channels.h"
#include "listmode.h"

/** Channel mode +b
 */
class ModeChannelBan : public ListModeBase
{
 public:
	ModeChannelBan()
		: ListModeBase(NULL, "ban", 'b', "End of channel ban list", 367, 368, true, "maxbans")
	{
	}
};

/** Channel mode +i
 */
class ModeChannelInviteOnly : public SimpleChannelModeHandler
{
 public:
	ModeChannelInviteOnly() : SimpleChannelModeHandler(NULL, "inviteonly", 'i')
	{
	}
};

/** Channel mode +k
 */
class ModeChannelKey : public ModeHandler
{
 public:
	ModeChannelKey();
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding);
	void RemoveMode(Channel* channel, irc::modestacker* stack = NULL);
	void RemoveMode(User* user, irc::modestacker* stack = NULL);
};


/** Channel mode +l
 */
class ModeChannelLimit : public ParamChannelModeHandler
{
 public:
	ModeChannelLimit();
	bool ParamValidate(std::string& parameter);
	bool ResolveModeConflict(std::string &their_param, const std::string &our_param, Channel* channel);
};

/** Channel mode +m
 */
class ModeChannelModerated : public SimpleChannelModeHandler
{
 public:
	ModeChannelModerated() : SimpleChannelModeHandler(NULL, "moderated", 'm')
	{
	}
};

/** Channel mode +n
 */
class ModeChannelNoExternal : public SimpleChannelModeHandler
{
 public:
	ModeChannelNoExternal() : SimpleChannelModeHandler(NULL, "noextmsg", 'n')
	{
	}
};

/** Channel mode +o
 */
class ModeChannelOp : public ModeHandler
{
 private:
 public:
	ModeChannelOp();
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding);
	unsigned int GetPrefixRank();
	void RemoveMode(Channel* channel, irc::modestacker* stack = NULL);
	void RemoveMode(User* user, irc::modestacker* stack = NULL);
};

/** Channel mode +p
 */
class ModeChannelPrivate : public SimpleChannelModeHandler
{
 public:
	ModeChannelPrivate() : SimpleChannelModeHandler(NULL, "private", 'p')
	{
	}
};

/** Channel mode +s
 */
class ModeChannelSecret : public SimpleChannelModeHandler
{
 public:
	ModeChannelSecret() : SimpleChannelModeHandler(NULL, "secret", 's')
	{
	}
};

/** Channel mode +t
 */
class ModeChannelTopicOps : public SimpleChannelModeHandler
{
 public:
	ModeChannelTopicOps() : SimpleChannelModeHandler(NULL, "topiclock", 't')
	{
	}
};

/** Channel mode +v
 */
class ModeChannelVoice : public ModeHandler
{
 private:
 public:
	ModeChannelVoice();
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding);
	unsigned int GetPrefixRank();
	void RemoveMode(User* user, irc::modestacker* stack = NULL);
	void RemoveMode(Channel* channel, irc::modestacker* stack = NULL);
};

/** User mode +i
 */
class ModeUserInvisible : public SimpleUserModeHandler
{
 public:
	ModeUserInvisible() : SimpleUserModeHandler(NULL, "invisible", 'i')
	{
	}
};

/** User mode +n
 */
class ModeUserServerNoticeMask : public ModeHandler
{
 public:
	ModeUserServerNoticeMask();
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding);
	void OnParameterMissing(User* user, User* dest, Channel* channel);
	std::string GetUserParameter(User* user);
};

/** User mode +o
 */
class ModeUserOperator : public ModeHandler
{
 public:
	ModeUserOperator();
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding);
};

/** User mode +w
 */
class ModeUserWallops : public SimpleUserModeHandler
{
 public:
	ModeUserWallops() : SimpleUserModeHandler(NULL, "wallops", 'w')
	{
	}
};
