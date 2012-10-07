/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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

#include "mode.h"

/** Channel mode +i
 */
class ModeChannelInviteOnly : public SimpleChannelModeHandler
{
 public:
	ModeChannelInviteOnly() : SimpleChannelModeHandler(NULL, "inviteonly", 'i')
	{
	}
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
      
/** User mode +i
 */
class ModeUserInvisible : public SimpleUserModeHandler
{
 public:
	ModeUserInvisible() : SimpleUserModeHandler(NULL, "invisible", 'i')
	{
	}
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
