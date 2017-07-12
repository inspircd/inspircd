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

/** Channel mode +k
 */
class ModeChannelKey : public ParamMode<ModeChannelKey, LocalStringExt>
{
	static const std::string::size_type maxkeylen = 32;
 public:
	ModeChannelKey();
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding);
	void SerializeParam(Channel* chan, const std::string* key, std::string& out);
	ModeAction OnSet(User* source, Channel* chan, std::string& param);
};

/** Channel mode +l
 */
class ModeChannelLimit : public ParamMode<ModeChannelLimit, LocalIntExt>
{
 public:
	ModeChannelLimit();
	bool ResolveModeConflict(std::string &their_param, const std::string &our_param, Channel* channel);
	void SerializeParam(Channel* chan, intptr_t n, std::string& out);
	ModeAction OnSet(User* source, Channel* channel, std::string& parameter);
};

/** Channel mode +o
 */
class ModeChannelOp : public PrefixMode
{
 public:
	ModeChannelOp()
		: PrefixMode(NULL, "op", 'o', OP_VALUE, '@')
	{
		levelrequired = OP_VALUE;
	}
};

/** Channel mode +v
 */
class ModeChannelVoice : public PrefixMode
{
 public:
	ModeChannelVoice()
		: PrefixMode(NULL, "voice", 'v', VOICE_VALUE, '+')
	{
		levelrequired = HALFOP_VALUE;
	}
};

/** User mode +s
 */
class ModeUserServerNoticeMask : public ModeHandler
{
	/** Process a snomask modifier string, e.g. +abc-de
	 * @param user The target user
	 * @param input A sequence of notice mask characters
	 * @return The cleaned mode sequence which can be output,
	 * e.g. in the above example if masks c and e are not
	 * valid, this function will return +ab-d
	 */
	std::string ProcessNoticeMasks(User* user, const std::string& input);

 public:
	ModeUserServerNoticeMask();
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding) CXX11_OVERRIDE;
	void OnParameterMissing(User* user, User* dest, Channel* channel) CXX11_OVERRIDE;

	/** Create a displayable mode string of the snomasks set on a given user
	 * @param user The user whose notice masks to format
	 * @return The notice mask character sequence
	 */
	std::string GetUserParameter(const User* user) const CXX11_OVERRIDE;
};

/** User mode +o
 */
class ModeUserOperator : public ModeHandler
{
 public:
	ModeUserOperator();
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding);
};
