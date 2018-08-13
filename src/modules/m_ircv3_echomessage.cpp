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

		// Caps are only set on local users
		LocalUser* const localuser = static_cast<LocalUser*>(user);

		const std::string& text = details.echooriginal ? details.originaltext : details.text;
		if (target.type == MessageTarget::TYPE_USER)
		{
			User* destuser = target.Get<User>();
			ClientProtocol::Messages::Privmsg privmsg(ClientProtocol::Messages::Privmsg::nocopy, user, destuser, text, details.type);
			privmsg.AddTags(details.tags_in);
			localuser->Send(ServerInstance->GetRFCEvents().privmsg, privmsg);
		}
		else if (target.type == MessageTarget::TYPE_CHANNEL)
		{
			Channel* chan = target.Get<Channel>();
			ClientProtocol::Messages::Privmsg privmsg(ClientProtocol::Messages::Privmsg::nocopy, user, chan, text, details.type, target.status);
			privmsg.AddTags(details.tags_in);
			localuser->Send(ServerInstance->GetRFCEvents().privmsg, privmsg);
		}
		else
		{
			const std::string* servername = target.Get<std::string>();
			ClientProtocol::Messages::Privmsg privmsg(ClientProtocol::Messages::Privmsg::nocopy, user, *servername, text, details.type);
			privmsg.AddTags(details.tags_in);
			localuser->Send(ServerInstance->GetRFCEvents().privmsg, privmsg);
		}
	}

	void OnUserMessageBlocked(User* user, const MessageTarget& target, const MessageDetails& details) CXX11_OVERRIDE
	{
		// Prevent spammers from knowing that their spam was blocked.
		if (details.echooriginal)
			OnUserPostMessage(user, target, details);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides the echo-message IRCv3.2 extension", VF_VENDOR);
	}
};

MODULE_INIT(ModuleIRCv3EchoMessage)
