/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2017-2020, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2015, 2018 Attila Molnar <attilamolnar@hush.com>
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
#include "clientprotocolmsg.h"
#include "modules/cap.h"
#include "modules/ctctags.h"

class EchoTag final
	: public ClientProtocol::MessageTagProvider
{
private:
	CTCTags::CapReference stdrplcap;

public:
	Cap::Capability echomsgcap;

	EchoTag(Module* Creator)
		: ClientProtocol::MessageTagProvider(Creator)
		, stdrplcap(Creator)
		, echomsgcap(Creator, "echo-message")
	{
	}

	bool ShouldSendTag(LocalUser* user, const ClientProtocol::MessageTagData& tagdata) final
	{
		return stdrplcap.IsEnabled(user) && echomsgcap.IsEnabled(user);
	}
};

class ModuleIRCv3EchoMessage final
	: public Module
	, public CTCTags::EventListener
{
private:
	EchoTag echotag;
	ClientProtocol::EventProvider tagmsgprov;

	void AddEchoTag(ClientProtocol::Message& msg)
	{
		msg.AddTag("inspircd.org/echo", &echotag, "");
	}

public:
	ModuleIRCv3EchoMessage()
		: Module(VF_VENDOR, "Provides the IRCv3 echo-message client capability.")
		, CTCTags::EventListener(this)
		, echotag(this)
		, tagmsgprov(this, "TAGMSG")
	{
	}

	void OnUserPostMessage(User* user, const MessageTarget& target, const MessageDetails& details) override
	{
		if (!echotag.echomsgcap.IsEnabled(user) || !details.echo)
			return;

		// Caps are only set on local users
		LocalUser* const localuser = static_cast<LocalUser*>(user);

		const std::string& text = details.echo_original ? details.original_text : details.text;
		const ClientProtocol::TagMap& tags = details.echo_original ? details.tags_in : details.tags_out;
		switch (target.type)
		{
			case MessageTarget::TYPE_USER:
			{
				User* destuser = target.Get<User>();
				ClientProtocol::Messages::Privmsg privmsg(ClientProtocol::Messages::Privmsg::nocopy, user, destuser, text, details.type);
				privmsg.AddTags(tags);
				AddEchoTag(privmsg);
				localuser->Send(ServerInstance->GetRFCEvents().privmsg, privmsg);
				break;
			}
			case MessageTarget::TYPE_CHANNEL:
			{
				Channel* chan = target.Get<Channel>();
				const char status = details.echo_original ? target.original_status : target.status;
				ClientProtocol::Messages::Privmsg privmsg(ClientProtocol::Messages::Privmsg::nocopy, user, chan, text, details.type, status);
				privmsg.AddTags(tags);
				AddEchoTag(privmsg);
				localuser->Send(ServerInstance->GetRFCEvents().privmsg, privmsg);
				break;
			}
			case MessageTarget::TYPE_SERVER:
			{
				const std::string* servername = target.Get<std::string>();
				ClientProtocol::Messages::Privmsg privmsg(ClientProtocol::Messages::Privmsg::nocopy, user, *servername, text, details.type);
				privmsg.AddTags(tags);
				AddEchoTag(privmsg);
				localuser->Send(ServerInstance->GetRFCEvents().privmsg, privmsg);
				break;
			}
		}
	}

	void OnUserPostTagMessage(User* user, const MessageTarget& target, const CTCTags::TagMessageDetails& details) override
	{
		if (!echotag.echomsgcap.IsEnabled(user) || !details.echo)
			return;

		// Caps are only set on local users
		LocalUser* const localuser = static_cast<LocalUser*>(user);

		const ClientProtocol::TagMap& tags = details.echo_original ? details.tags_in : details.tags_out;
		switch (target.type)
		{
			case MessageTarget::TYPE_USER:
			{
				User* destuser = target.Get<User>();
				CTCTags::TagMessage message(user, destuser, tags);
				AddEchoTag(message);
				localuser->Send(tagmsgprov, message);
				break;
			}
			case MessageTarget::TYPE_CHANNEL:
			{
				Channel* chan = target.Get<Channel>();
				const char status = details.echo_original ? target.original_status : target.status;
				CTCTags::TagMessage message(user, chan, tags, status);
				AddEchoTag(message);
				localuser->Send(tagmsgprov, message);
				break;
			}
			case MessageTarget::TYPE_SERVER:
			{
				const std::string* servername = target.Get<std::string>();
				CTCTags::TagMessage message(user, servername->c_str(), tags);
				AddEchoTag(message);
				localuser->Send(tagmsgprov, message);
				break;
			}
		}
	}

	void OnUserMessageBlocked(User* user, const MessageTarget& target, const MessageDetails& details) override
	{
		// Prevent spammers from knowing that their spam was blocked.
		if (details.echo_original)
			OnUserPostMessage(user, target, details);
	}

	void OnUserTagMessageBlocked(User* user, const MessageTarget& target, const CTCTags::TagMessageDetails& details) override
	{
		// Prevent spammers from knowing that their spam was blocked.
		if (details.echo_original)
			OnUserPostTagMessage(user, target, details);
	}
};

MODULE_INIT(ModuleIRCv3EchoMessage)
