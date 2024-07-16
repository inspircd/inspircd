/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017, 2019, 2021-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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
#include "numerichelper.h"

#include "core_channel.h"

enum
{
	// From RFC 1459.
	RPL_BANLIST = 367,
	RPL_ENDOFBANLIST = 368,
};

ModeChannelBan::ModeChannelBan(Module* Creator)
	: ListModeBase(Creator, "ban", 'b', RPL_BANLIST, RPL_ENDOFBANLIST)
	, extbanmgr(Creator)
{
	syntax = "<mask>";
}

bool ModeChannelBan::CompareEntry(const std::string& entry, const std::string& value) const
{
	if (extbanmgr)
	{
		auto res = extbanmgr->CompareEntry(this, entry, value);
		if (res != ExtBan::Comparison::NOT_AN_EXTBAN)
			return res == ExtBan::Comparison::MATCH;
	}
	return irc::equals(entry, value);
}

bool ModeChannelBan::ValidateParam(LocalUser* user, Channel* channel, std::string& parameter)
{
	if (!extbanmgr || !extbanmgr->Canonicalize(parameter))
		ModeParser::CleanMask(parameter);
	return true;
}

ModeChannelLimit::ModeChannelLimit(Module* Creator)
	: ParamMode<ModeChannelLimit, IntExtItem>(Creator, "limit", 'l')
{
	syntax = "<limit>";
}

bool ModeChannelLimit::ResolveModeConflict(const std::string& their_param, const std::string& our_param, Channel* channel)
{
	// When the timestamps are equal the higher channel limit wins.
	return ConvToNum<intptr_t>(their_param) < ConvToNum<intptr_t>(our_param);
}

bool ModeChannelLimit::OnSet(User* user, Channel* chan, std::string& parameter)
{
	size_t limit = ConvToNum<size_t>(parameter);
	if (limit < 1 || limit > INTPTR_MAX)
	{
		if (IS_LOCAL(user))
		{
			// If the setter is local then we can safely just reject this here.
			user->WriteNumeric(Numerics::InvalidModeParameter(chan, this, parameter));
			return false;
		}
		else
		{
			// If the setter is remote we *must* set the mode to avoid a desync
			// so instead clamp it to the allowed range instead.
			limit = std::clamp<size_t>(limit, 1, INTPTR_MAX);
		}
	}

	ext.Set(chan, limit);
	return true;
}

void ModeChannelLimit::SerializeParam(Channel* chan, intptr_t limit, std::string& out)
{
	out += ConvToStr(static_cast<size_t>(limit));
}

ModeChannelOp::ModeChannelOp(Module* Creator)
	: PrefixMode(Creator, "op", 'o', OP_VALUE, '@')
{
	ranktoset = ranktounset = OP_VALUE;
}

ModeChannelVoice::ModeChannelVoice(Module* Creator)
	: PrefixMode(Creator, "voice", 'v', VOICE_VALUE, '+')
{
	selfremove = false;
	ranktoset = ranktounset = HALFOP_VALUE;
}
