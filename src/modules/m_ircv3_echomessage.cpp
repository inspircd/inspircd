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

	void OnUserPostMessage(User* user, const MessageTarget& target, const MessageDetails& details) CXX11_OVERRIDE
	{
		if (!cap.get(user))
			return;

		std::string msg = MessageTypeStringSp[details.type];
		if (target.type == MessageTarget::TYPE_USER)
		{
			User* destuser = target.Get<User>();
			msg.append(destuser->nick);
		}
		else if (target.type == MessageTarget::TYPE_CHANNEL)
		{
			if (target.status)
				msg.push_back(target.status);

			Channel* chan = target.Get<Channel>();
			msg.append(chan->name);
		}
		else
		{
			const std::string* servername = target.Get<std::string>();
			msg.append(*servername);
		}
		msg.append(" :").append(details.echooriginal ? details.originaltext : details.text);
		user->WriteFrom(user, msg);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides the echo-message IRCv3.2 extension", VF_VENDOR);
	}
};

MODULE_INIT(ModuleIRCv3EchoMessage)
