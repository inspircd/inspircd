/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020-2023 Sadie Powell <sadie@witchery.services>
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

bool ChannelManager::DefaultIsChannel(const std::string_view& channel)
{
	if (channel.empty() || channel.length() > ServerInstance->Config->Limits.MaxChannel)
		return false;

	if (!ServerInstance->Channels.IsPrefix(channel[0]))
		return false;

	for (const auto chr : insp::iterator_range(channel.begin() + 1, channel.end()))
	{
		switch (chr)
		{
			case 0x07: // BELL
			case 0x20: // SPACE
			case 0x2C: // COMMA
				return false;
		}
	}

	return true;
}

Channel* ChannelManager::Find(const std::string& channel) const
{
	ChannelMap::const_iterator iter = channels.find(channel);
	if (iter == channels.end())
		return nullptr;

	return iter->second;
}

bool ChannelManager::IsPrefix(unsigned char prefix) const
{
	// TODO: implement support for multiple channel types.
	return prefix == '#';
}
