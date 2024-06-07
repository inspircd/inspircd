/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013-2014, 2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2018, 2020, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005 Craig Edwards <brain@inspircd.org>
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

class Channel;
class ConfigStatus;
class ConfigTag;
class Extensible;
class FakeUser;
class InspIRCd;
class IOHookProvider;
class LocalUser;
class ModResult;
class Module;
class OperAccount;
class OperType;
class RemoteUser;
class Server;
class User;
class XLine;
class XLineFactory;
class XLineManager;

namespace ClientProtocol
{
	class Event;
	class EventProvider;
	class Message;
	class MessageTagEvent;
	class MessageTagProvider;
	class Serializer;

	typedef std::vector<Message*> MessageList;
	typedef std::vector<std::string> ParamList;
	typedef std::string SerializedMessage;

	struct CoreExport MessageTagData final
	{
		MessageTagProvider* tagprov;
		std::string value;
		void* provdata;

		MessageTagData(MessageTagProvider* prov, const std::string& val, void* data = nullptr);
	};

	/** Map of message tag values and providers keyed by their name.
	 * Sorted in descending order to ensure tag names beginning with symbols (such as '+') come later when iterating
	 * the container than tags with a normal name.
	 */
	typedef insp::flat_map<std::string, MessageTagData, std::greater<>> TagMap;
}

#include "hashcomp.h"
#include "base.h"

/** Generic user list, used for exceptions */
typedef std::set<User*> CUList;

/** A bitset of characters which are enabled/set. */
typedef std::bitset<UCHAR_MAX + 1> CharState;
