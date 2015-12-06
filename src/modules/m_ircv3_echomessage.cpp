/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013-2015 Peter Powell <petpow@saberuk.com>
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
#include "modules/cap.h"

static const char* MessageTypeStringSp[] = { "PRIVMSG ", "NOTICE " };

class ModuleIRCv3EchoMessage : public Module
{
	Cap::Capability cap;

 public:
	ModuleIRCv3EchoMessage()
		: cap(this, "echo-message")
	{
	}

	void OnUserMessage(User* user, void* dest, int target_type, const std::string& text, char status, const CUList& exempt_list, MessageType msgtype) CXX11_OVERRIDE
	{
		if (!cap.get(user))
			return;

		std::string msg = MessageTypeStringSp[msgtype];
		if (target_type == TYPE_USER)
		{
			User* destuser = static_cast<User*>(dest);
			msg.append(destuser->nick);
		}
		else if (target_type == TYPE_CHANNEL)
		{
			if (status)
				msg.push_back(status);

			Channel* chan = static_cast<Channel*>(dest);
			msg.append(chan->name);
		}
		else
		{
			const char* servername = static_cast<const char*>(dest);
			msg.append(servername);
		}
		msg.append(" :").append(text);
		user->WriteFrom(user, msg);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides the echo-message IRCv3.2 extension", VF_VENDOR);
	}
};

MODULE_INIT(ModuleIRCv3EchoMessage)
