/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2022-2024 Sadie Powell <sadie@witchery.services>
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

enum
{
	// From RFC 1459.
	ERR_CANNOTSENDTOCHAN = 404,

	// InspIRCd-specific.
	ERR_CANNOTSENDTOUSER = 531,
	ERR_INVALIDMODEPARAM = 696,
};

namespace Numerics
{
	class CannotSendTo;
	class ChannelPrivilegesNeeded;
	class InvalidModeParameter;
	class NoSuchChannel;
	class NoSuchNick;
}

/** Helper for the ERR_CANNOTSENDTOCHAN and ERR_CANNOTSENDTOUSER numerics. */
class Numerics::CannotSendTo final
	: public Numeric::Numeric
{
public:
	CannotSendTo(const Channel* chan, const std::string& message)
		: Numeric(ERR_CANNOTSENDTOCHAN)
	{
		push(chan->name);
		push(message);
	}

	CannotSendTo(const Channel* chan, const std::string& what, const ModeHandler* mh)
		: Numeric(ERR_CANNOTSENDTOCHAN)
	{
		push(chan->name);
		push_fmt("You cannot send {} to this channel whilst the +{} ({}) mode is set.", what,
			mh->GetModeChar(), mh->name);
	}

#ifdef INSPIRCD_EXTBAN
	CannotSendTo(const Channel* chan, const std::string& what, const ExtBan::Base& xb)
		: Numeric(ERR_CANNOTSENDTOCHAN)
	{
		push(chan->name);
		push_fmt("You cannot send {} to this channel whilst {} {}: ({}) extban is set matching you.",
			what, strchr("AEIOUaeiou", xb.GetLetter()) ? "an" : "a", xb.GetLetter(), xb.GetName());
	}
#endif

	CannotSendTo(const User* user, const std::string& message)
		: Numeric(ERR_CANNOTSENDTOUSER)
	{
		push(user->connected & User::CONN_NICK ? user->nick : "*");
		push(message);
	}

	CannotSendTo(const User* user, const std::string& what, const ModeHandler* mh, bool self = false)
		: Numeric(ERR_CANNOTSENDTOUSER)
	{
		push(user->connected & User::CONN_NICK ? user->nick : "*");
		push_fmt("You cannot send {} to this user whilst {} have the +{} ({}) mode set.",
			what, self ? "you" : "they", mh->GetModeChar(), mh->name);
	}
};

/* Helper for the ERR_CHANOPRIVSNEEDED numeric. */
class Numerics::ChannelPrivilegesNeeded : public Numeric::Numeric
{
public:
	ChannelPrivilegesNeeded(const Channel* chan, ModeHandler::Rank rank, const std::string& message)
		: Numeric(ERR_CHANOPRIVSNEEDED)
	{
		push(chan->name);

		const auto* pm = ServerInstance->Modes.FindNearestPrefixMode(rank);
		if (pm)
			push_fmt("You must be a channel {} or higher to {}.", pm->name, message);
		else
			push_fmt("You do not have the required channel privileges to {}.", message);
	}
};

/* Helper for the ERR_INVALIDMODEPARAM numeric. */
class Numerics::InvalidModeParameter final
	: public Numeric::Numeric
{
private:
	void push_message(const ModeHandler* mode, const std::string& message)
	{
		if (!message.empty())
		{
			// The caller has specified their own message.
			push(message);
			return;
		}

		const std::string& syntax = mode->GetSyntax();
		if (!syntax.empty())
		{
			// If the mode has a syntax hint we include it in the message.
			push_fmt("Invalid {} mode parameter. Syntax: {}.", mode->name, syntax);
		}
		else
		{
			// Otherwise, send it without.
			push_fmt("Invalid {} mode parameter.", mode->name);
		}
	}

public:
	InvalidModeParameter(const Channel* chan, const ModeHandler* mode, const std::string& parameter, const std::string& message = "")
		: Numeric(ERR_INVALIDMODEPARAM)
	{
		push(chan->name);
		push(mode->GetModeChar());
		push(parameter);
		push_message(mode, message);
	}

	InvalidModeParameter(const User* user, const ModeHandler* mode, const std::string& parameter, const std::string& message = "")
		: Numeric(ERR_INVALIDMODEPARAM)
	{
		push(user->connected & User::CONN_NICK ? user->nick : "*");
		push(mode->GetModeChar());
		push(parameter);
		push_message(mode, message);
	}
};

/** Helper for the ERR_NOSUCHCHANNEL numeric. */
class Numerics::NoSuchChannel final
	: public Numeric::Numeric
{
public:
	NoSuchChannel(const std::string& chan)
		: Numeric(ERR_NOSUCHCHANNEL)
	{
		push(chan.empty() ? "*" : chan);
		push("No such channel");
	}
};

/** Helper for the ERR_NOSUCHNICK numeric. */
class Numerics::NoSuchNick final
	: public Numeric::Numeric
{
public:
	NoSuchNick(const std::string& nick)
		: Numeric(ERR_NOSUCHNICK)
	{
		push(nick.empty() ? "*" : nick);
		push("No such nick");
	}
};
