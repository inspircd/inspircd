/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020-2022 Sadie Powell <sadie@witchery.services>
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

/** A mapping of channel names to their Channel object. */
typedef std::unordered_map<std::string, Channel*, irc::insensitive, irc::StrHashComp> ChannelMap;

/** Manages state relating to channels. */
class CoreExport ChannelManager final
{
private:
	/** A map of channel names to the channel object. */
	ChannelMap channels;

public:
	/** Determines whether an channel name is valid. */
	std::function<bool(const std::string_view&)> IsChannel = DefaultIsChannel;

	/** Determines whether a channel name is valid according to the RFC 1459 rules.
	 * This is the default function for InspIRCd::IsChannel.
	 * @param channel The channel name to validate.
	 * @return True if the channel name is valid according to RFC 1459 rules; otherwise, false.
	 */
	static bool DefaultIsChannel(const std::string_view& channel);

	/** Finds a channel by name.
	 * @param channel The name of the channel to look up.
	 * @return If the channel was found then a pointer to a Channel object; otherwise, nullptr.
	 */
	Channel* Find(const std::string& channel) const;

	/** Retrieves a map containing all channels keyed by the channel name. */
	ChannelMap& GetChans() { return channels; }
	const ChannelMap& GetChans() const { return channels; }

	/** Determines whether the specified character is a valid channel prefix.
	 * @param prefix The channel name prefix to validate.
	 * @return True if the character is a channel prefix; otherwise, false.
	 */
	bool IsPrefix(unsigned char prefix) const;
};
