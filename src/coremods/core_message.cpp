/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2014, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2008 Craig Edwards <brain@inspircd.org>
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
#include "numericbuilder.h"
#include "numerichelper.h"
#include "utility/string.h"

class MessageDetailsImpl final
	: public MessageDetails
{
public:
	MessageDetailsImpl(MessageType mt, const std::string& msg, const ClientProtocol::TagMap& tags)
		: MessageDetails(mt, msg, tags)
	{
	}

	bool IsCTCP(std::string_view& name, std::string_view& body) const override
	{
		if (!this->IsCTCP())
			return false;

		size_t end_of_name = text.find(' ', 2);
		size_t end_of_ctcp = text.back() == '\x1' ? 1 : 0;
		if (end_of_name == std::string::npos)
		{
			// The CTCP only contains a name.
			name = insp::substring_view(text, text.begin() + 1, text.end() - end_of_ctcp);
			body = std::string_view();
			return true;
		}

		// The CTCP contains a name and a body.
		name = insp::substring_view(text, text.begin() + 1, text.begin() + end_of_name);

		size_t start_of_body = text.find_first_not_of(' ', end_of_name + 1);
		if (start_of_body == std::string::npos)
		{
			// The CTCP body is provided but empty.
			body = std::string_view();
			return true;
		}

		// The CTCP body provided was non-empty.
		body = insp::substring_view(text, text.begin() + start_of_body, text.end() - end_of_ctcp);
		return true;
	}

	bool IsCTCP(std::string_view& name) const override
	{
		if (!this->IsCTCP())
			return false;

		size_t end_of_name = text.find(' ', 2);
		if (end_of_name == std::string::npos)
		{
			// The CTCP only contains a name.
			size_t end_of_ctcp = text.back() == '\x1' ? 1 : 0;
			name = insp::substring_view(text, text.begin() + 1, text.end() - end_of_ctcp);
			return true;
		}

		// The CTCP contains a name and a body.
		name = insp::substring_view(text, text.begin() + 1, text.begin() + end_of_name);
		return true;
	}

	bool IsCTCP() const override
	{
		// According to draft-oakley-irc-ctcp-02 a valid CTCP must begin with SOH and
		// contain at least one octet which is not NUL, SOH, CR, LF, or SPACE. As most
		// of these are restricted at the protocol level we only need to check for SOH
		// and SPACE.
		return (text.length() >= 2) && (text[0] == '\x1') &&  (text[1] != '\x1') && (text[1] != ' ');
	}
};

class CommandMessage final
	: public Command
{
private:
	const MessageType msgtype;

	static bool FirePreEvents(User* source, MessageTarget& msgtarget, MessageDetails& msgdetails)
	{
		// Inform modules that a message wants to be sent.
		ModResult modres;
		FIRST_MOD_RESULT(OnUserPreMessage, modres, (source, msgtarget, msgdetails));
		if (modres == MOD_RES_DENY)
		{
			// Inform modules that a module blocked the message.
			FOREACH_MOD(OnUserMessageBlocked, (source, msgtarget, msgdetails));
			return false;
		}

		// Check whether a module zapped the message body.
		if (msgdetails.text.empty())
		{
			source->WriteNumeric(ERR_NOTEXTTOSEND, "No text to send");
			return false;
		}

		// Inform modules that a message is about to be sent.
		FOREACH_MOD(OnUserMessage, (source, msgtarget, msgdetails));
		return true;
	}

	static CmdResult FirePostEvent(User* source, const MessageTarget& msgtarget, const MessageDetails& msgdetails)
	{
		// If the source is local and was not sending a CTCP reply then update their idle time.
		LocalUser* lsource = IS_LOCAL(source);
		if (lsource && msgdetails.update_idle && (msgdetails.type != MessageType::NOTICE || !msgdetails.IsCTCP()))
			lsource->idle_lastmsg = ServerInstance->Time();

		// Inform modules that a message was sent.
		FOREACH_MOD(OnUserPostMessage, (source, msgtarget, msgdetails));
		return CmdResult::SUCCESS;
	}

	CmdResult HandleChannelTarget(User* source, const Params& parameters, const char* target, PrefixMode* pm)
	{
		auto* chan = ServerInstance->Channels.Find(target);
		if (!chan)
		{
			// The target channel does not exist.
			source->WriteNumeric(Numerics::NoSuchChannel(parameters[0]));
			return CmdResult::FAILURE;
		}

		// Fire the pre-message events.
		MessageTarget msgtarget(chan, pm ? pm->GetPrefix() : 0);
		MessageDetailsImpl msgdetails(msgtype, parameters[1], parameters.GetTags());
		msgdetails.exemptions.insert(source);
		if (!FirePreEvents(source, msgtarget, msgdetails))
			return CmdResult::FAILURE;

		// Send the message to the members of the channel.
		ClientProtocol::Messages::Privmsg privmsg(ClientProtocol::Messages::Privmsg::nocopy, source, chan, msgdetails.text, msgdetails.type, msgtarget.status);
		privmsg.AddTags(msgdetails.tags_out);
		privmsg.SetSideEffect(true);
		chan->Write(ServerInstance->GetRFCEvents().privmsg, privmsg, msgtarget.status, msgdetails.exemptions);

		// Create the outgoing message and message event.
		return FirePostEvent(source, msgtarget, msgdetails);
	}

	CmdResult HandleServerTarget(User* source, const Params& parameters)
	{
		// If the source isn't allowed to mass message users then reject
		// the attempt to mass-message users.
		if (!source->HasPrivPermission("users/mass-message"))
		{
			source->WriteNumeric(ERR_NOPRIVILEGES, "Permission Denied - You do not have the required operator privileges");
			return CmdResult::FAILURE;
		}

		// Extract the server glob match from the target parameter.
		std::string servername(parameters[0], 1);

		// Fire the pre-message events.
		MessageTarget msgtarget(&servername);
		MessageDetailsImpl msgdetails(msgtype, parameters[1], parameters.GetTags());
		if (!FirePreEvents(source, msgtarget, msgdetails))
			return CmdResult::FAILURE;

		// If the current server name matches the server name glob then send
		// the message out to the local users.
		if (InspIRCd::Match(ServerInstance->Config->ServerName, servername))
		{
			// Create the outgoing message and message event.
			ClientProtocol::Messages::Privmsg message(ClientProtocol::Messages::Privmsg::nocopy, source, "$*", msgdetails.text, msgdetails.type);
			message.AddTags(msgdetails.tags_out);
			message.SetSideEffect(true);
			ClientProtocol::Event messageevent(ServerInstance->GetRFCEvents().privmsg, message);

			for (auto* luser : ServerInstance->Users.GetLocalUsers())
			{
				// Don't send to partially connected users or the user who is the source.
				if (!luser->IsFullyConnected() || luser == source)
					continue;

				// Only send to non-exempt users.
				if (!msgdetails.exemptions.count(luser))
					luser->Send(messageevent);
			}
		}

		// Fire the post-message event.
		return FirePostEvent(source, msgtarget, msgdetails);
	}

	CmdResult HandleUserTarget(User* source, const Params& parameters)
	{
		User* target;
		if (IS_LOCAL(source))
		{
			// Local sources can specify either a nick or a nick@server mask as the target.
			const char* targetserver = strchr(parameters[0].c_str(), '@');
			if (targetserver)
			{
				// The target is a user on a specific server (e.g. jto@tolsun.oulu.fi).
				target = ServerInstance->Users.FindNick(parameters[0].substr(0, targetserver - parameters[0].c_str()), true);
				if (target && strcasecmp(target->server->GetPublicName().c_str(), targetserver + 1) != 0)
					target = nullptr;
			}
			else
			{
				// If the source is a local user then we only look up the target by nick.
				target = ServerInstance->Users.FindNick(parameters[0], true);
			}
		}
		else
		{
			// Remote users can only specify a nick or UUID as the target.
			target = ServerInstance->Users.Find(parameters[0], true);
		}

		if (!target)
		{
			// The target user does not exist or is not fully connected.
			source->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
			return CmdResult::FAILURE;
		}

		// Fire the pre-message events.
		MessageTarget msgtarget(target);
		MessageDetailsImpl msgdetails(msgtype, parameters[1], parameters.GetTags());
		if (!FirePreEvents(source, msgtarget, msgdetails))
			return CmdResult::FAILURE;

		// If the target is away then inform the user.
		if (target->IsAway() && msgdetails.type == MessageType::PRIVMSG)
			source->WriteNumeric(RPL_AWAY, target->nick, target->away->message);

		LocalUser* const localtarget = IS_LOCAL(target);
		if (localtarget)
		{
			// Servers can fake the target of a message when it is sent to an individual user.
			auto context_tag = parameters.GetTags().find("~context");
			const std::string& msgcontext = context_tag == parameters.GetTags().end()
				? localtarget->nick : context_tag->second.value;

			// Send to the target if they are a local user.
			ClientProtocol::Messages::Privmsg privmsg(ClientProtocol::Messages::Privmsg::nocopy, source, msgcontext, msgdetails.text, msgdetails.type);
			privmsg.AddTags(msgdetails.tags_out);
			privmsg.SetSideEffect(true);
			localtarget->Send(ServerInstance->GetRFCEvents().privmsg, privmsg);
		}

		// Fire the post-message event.
		return FirePostEvent(source, msgtarget, msgdetails);
	}

public:
	CommandMessage(Module* parent, MessageType mt)
		: Command(parent, ClientProtocol::Messages::Privmsg::CommandStrFromMsgType(mt), 2, 2)
		, msgtype(mt)
	{
		allow_empty_last_param = true;
		syntax = { "<target>[,<target>]+ :<message>" };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		if (CommandParser::LoopCall(user, this, parameters, 0))
			return CmdResult::SUCCESS;

		// The specified message was empty.
		if (parameters[1].empty())
		{
			user->WriteNumeric(ERR_NOTEXTTOSEND, "No text to send");
			return CmdResult::FAILURE;
		}

		// The target is a server glob.
		if (parameters[0][0] == '$')
			return HandleServerTarget(user, parameters);

		// If the message begins with one or more status characters then look them up.
		const char* target = parameters[0].c_str();
		PrefixMode* targetpfx = nullptr;
		for (PrefixMode* pfx; (pfx = ServerInstance->Modes.FindPrefix(target[0])); ++target)
		{
			// We want the lowest ranked prefix specified.
			if (!targetpfx || pfx->GetPrefixRank() < targetpfx->GetPrefixRank())
				targetpfx = pfx;
		}

		if (!target[0])
		{
			// The target consisted solely of prefix modes.
			user->WriteNumeric(ERR_NORECIPIENT, "No recipient given");
			return CmdResult::FAILURE;
		}

		// The target is a channel name.
		if (ServerInstance->Channels.IsPrefix(*target))
			return HandleChannelTarget(user, parameters, target, targetpfx);

		// The target is a nickname.
		return HandleUserTarget(user, parameters);
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		if (IS_LOCAL(user))
			// This is handled by the OnUserPostMessage hook to split the LoopCall pieces
			return ROUTE_LOCALONLY;
		else
			return ROUTE_MESSAGE(parameters[0]);
	}
};

class CommandSQuery final
	: public Command
{
public:
	CommandSQuery(Module* Creator)
		: Command(Creator, "SQUERY", 2, 2)
	{
		allow_empty_last_param = true;
		syntax = { "<service> :<message>" };
		translation = { TR_NICK, TR_TEXT };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		// The specified message was empty.
		if (parameters[1].empty())
		{
			user->WriteRemoteNumeric(ERR_NOTEXTTOSEND, "No text to send");
			return CmdResult::FAILURE;
		}

		User* target;
		if (IS_LOCAL(user))
		{
			// Local sources can specify either a nick or a nick@server mask as the target.
			const char* targetserver = strchr(parameters[0].c_str(), '@');
			if (targetserver)
			{
				// The target is a user on a specific server (e.g. jto@tolsun.oulu.fi).
				target = ServerInstance->Users.FindNick(parameters[0].substr(0, targetserver - parameters[0].c_str()), true);
				if (target && strcasecmp(target->server->GetPublicName().c_str(), targetserver + 1) != 0)
					target = nullptr;
			}
			else
			{
				// If the source is a local user then we only look up the target by nick.
				target = ServerInstance->Users.FindNick(parameters[0], true);
			}
		}
		else
		{
			// Remote users can only specify a nick or UUID as the target.
			target = ServerInstance->Users.Find(parameters[0], true);
		}

		if (!target || !target->server->IsService())
		{
			// The target user does not exist, is not fully connected, or is not a service.
			user->WriteRemoteNumeric(ERR_NOSUCHSERVICE, parameters[0], "No such service");
			return CmdResult::FAILURE;
		}

		// The SQUERY command targets a service on a services server. This can
		// never be on the server local to the source so we can just unicast it
		// to the server in question.
		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return ROUTE_UNICAST(parameters[0]);
	}
};

class ModuleCoreMessage final
	: public Module
{
private:
	CommandMessage cmdprivmsg;
	CommandMessage cmdnotice;
	CommandSQuery cmdsquery;
	ChanModeReference moderatedmode;
	ChanModeReference noextmsgmode;

public:
	ModuleCoreMessage()
		: Module(VF_CORE | VF_VENDOR, "Provides the NOTICE, PRIVMSG, and SQUERY commands")
		, cmdprivmsg(this, MessageType::PRIVMSG)
		, cmdnotice(this, MessageType::NOTICE)
		, cmdsquery(this)
		, moderatedmode(this, "moderated")
		, noextmsgmode(this, "noextmsg")
	{
	}

	ModResult OnUserPreMessage(User* user, MessageTarget& target, MessageDetails& details) override
	{
		if (!IS_LOCAL(user) || target.type != MessageTarget::TYPE_CHANNEL)
			return MOD_RES_PASSTHRU;

		Channel* chan = target.Get<Channel>();
		if (chan->IsModeSet(noextmsgmode) && !chan->HasUser(user))
		{
			// The noextmsg mode is set and the user is not in the channel.
			user->WriteNumeric(Numerics::CannotSendTo(chan, "external messages", *noextmsgmode));
			return MOD_RES_DENY;
		}

		bool no_chan_priv = chan->GetPrefixValue(user) < VOICE_VALUE;
		if (no_chan_priv && chan->IsModeSet(moderatedmode))
		{
			// The moderated mode is set and the user has no status rank.
			user->WriteNumeric(Numerics::CannotSendTo(chan, "messages", *moderatedmode));
			return MOD_RES_DENY;
		}

		if (no_chan_priv && ServerInstance->Config->RestrictBannedUsers != ServerConfig::BUT_NORMAL && chan->IsBanned(user))
		{
			// The user is banned in the channel and restrictbannedusers is enabled.
			if (ServerInstance->Config->RestrictBannedUsers == ServerConfig::BUT_RESTRICT_NOTIFY)
				user->WriteNumeric(Numerics::CannotSendTo(chan, "You cannot send messages to this channel whilst banned."));
			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleCoreMessage)
