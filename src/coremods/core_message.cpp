/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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

enum
{
	// From RFC 2812.
	ERR_NOSUCHSERVICE = 408
};

class MessageDetailsImpl : public MessageDetails
{
public:
	MessageDetailsImpl(MessageType mt, const std::string& msg, const ClientProtocol::TagMap& tags)
		: MessageDetails(mt, msg, tags)
	{
	}

	bool IsCTCP(std::string& name, std::string& body) const CXX11_OVERRIDE
	{
		if (!this->IsCTCP())
			return false;

		size_t end_of_name = text.find(' ', 2);
		size_t end_of_ctcp = *text.rbegin() == '\x1' ? 1 : 0;
		if (end_of_name == std::string::npos)
		{
			// The CTCP only contains a name.
			name.assign(text, 1, text.length() - 1 - end_of_ctcp);
			body.clear();
			return true;
		}

		// The CTCP contains a name and a body.
		name.assign(text, 1, end_of_name - 1);

		size_t start_of_body = text.find_first_not_of(' ', end_of_name + 1);
		if (start_of_body == std::string::npos)
		{
			// The CTCP body is provided but empty.
			body.clear();
			return true;
		}

		// The CTCP body provided was non-empty.
		body.assign(text, start_of_body, text.length() - start_of_body - end_of_ctcp);
		return true;
	}

	bool IsCTCP(std::string& name) const CXX11_OVERRIDE
	{
		if (!this->IsCTCP())
			return false;

		size_t end_of_name = text.find(' ', 2);
		if (end_of_name == std::string::npos)
		{
			// The CTCP only contains a name.
			size_t end_of_ctcp = *text.rbegin() == '\x1' ? 1 : 0;
			name.assign(text, 1, text.length() - 1 - end_of_ctcp);
			return true;
		}

		// The CTCP contains a name and a body.
		name.assign(text, 1, end_of_name - 1);
		return true;
	}

	bool IsCTCP() const CXX11_OVERRIDE
	{
		// According to draft-oakley-irc-ctcp-02 a valid CTCP must begin with SOH and
		// contain at least one octet which is not NUL, SOH, CR, LF, or SPACE. As most
		// of these are restricted at the protocol level we only need to check for SOH
		// and SPACE.
		return (text.length() >= 2) && (text[0] == '\x1') &&  (text[1] != '\x1') && (text[1] != ' ');
	}
};

namespace
{
	bool FirePreEvents(User* source, MessageTarget& msgtarget, MessageDetails& msgdetails)
	{
		// Inform modules that a message wants to be sent.
		ModResult modres;
		FIRST_MOD_RESULT(OnUserPreMessage, modres, (source, msgtarget, msgdetails));
		if (modres == MOD_RES_DENY)
		{
			// Inform modules that a module blocked the mssage.
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

	CmdResult FirePostEvent(User* source, const MessageTarget& msgtarget, const MessageDetails& msgdetails)
	{
		// If the source is local and was not sending a CTCP reply then update their idle time.
		LocalUser* lsource = IS_LOCAL(source);
		if (lsource && (msgdetails.type != MSG_NOTICE || !msgdetails.IsCTCP()))
			lsource->idle_lastmsg = ServerInstance->Time();

		// Inform modules that a message was sent.
		FOREACH_MOD(OnUserPostMessage, (source, msgtarget, msgdetails));
		return CMD_SUCCESS;
	}
}

class CommandMessage : public Command
{
 private:
	const MessageType msgtype;
	ChanModeReference moderatedmode;
	ChanModeReference noextmsgmode;

	CmdResult HandleChannelTarget(User* source, const Params& parameters, const char* target, PrefixMode* pm)
	{
		Channel* chan = ServerInstance->FindChan(target);
		if (!chan)
		{
			// The target channel does not exist.
			source->WriteNumeric(Numerics::NoSuchChannel(parameters[0]));
			return CMD_FAILURE;
		}

		if (IS_LOCAL(source))
		{
			if (chan->IsModeSet(noextmsgmode) && !chan->HasUser(source))
			{
				// The noextmsg mode is set and the source is not in the channel.
				source->WriteNumeric(ERR_CANNOTSENDTOCHAN, chan->name, "Cannot send to channel (no external messages)");
				return CMD_FAILURE;
			}

			bool no_chan_priv = chan->GetPrefixValue(source) < VOICE_VALUE;
			if (no_chan_priv && chan->IsModeSet(moderatedmode))
			{
				// The moderated mode is set and the source has no status rank.
				source->WriteNumeric(ERR_CANNOTSENDTOCHAN, chan->name, "Cannot send to channel (+m)");
				return CMD_FAILURE;
			}

			if (no_chan_priv && ServerInstance->Config->RestrictBannedUsers != ServerConfig::BUT_NORMAL && chan->IsBanned(source))
			{
				// The source is banned in the channel and restrictbannedusers is enabled.
				if (ServerInstance->Config->RestrictBannedUsers == ServerConfig::BUT_RESTRICT_NOTIFY)
					source->WriteNumeric(ERR_CANNOTSENDTOCHAN, chan->name, "Cannot send to channel (you're banned)");
				return CMD_FAILURE;
			}
		}

		// Fire the pre-message events.
		MessageTarget msgtarget(chan, pm ? pm->GetPrefix() : 0);
		MessageDetailsImpl msgdetails(msgtype, parameters[1], parameters.GetTags());
		msgdetails.exemptions.insert(source);
		if (!FirePreEvents(source, msgtarget, msgdetails))
			return CMD_FAILURE;

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
			return CMD_FAILURE;

		// Extract the server glob match from the target parameter.
		std::string servername(parameters[0], 1);

		// Fire the pre-message events.
		MessageTarget msgtarget(&servername);
		MessageDetailsImpl msgdetails(msgtype, parameters[1], parameters.GetTags());
		if (!FirePreEvents(source, msgtarget, msgdetails))
			return CMD_FAILURE;

		// If the current server name matches the server name glob then send
		// the message out to the local users.
		if (InspIRCd::Match(ServerInstance->Config->ServerName, servername))
		{
			// Create the outgoing message and message event.
			ClientProtocol::Messages::Privmsg message(ClientProtocol::Messages::Privmsg::nocopy, source, "$*", msgdetails.text, msgdetails.type);
			message.AddTags(msgdetails.tags_out);
			message.SetSideEffect(true);
			ClientProtocol::Event messageevent(ServerInstance->GetRFCEvents().privmsg, message);

			const UserManager::LocalList& list = ServerInstance->Users.GetLocalUsers();
			for (UserManager::LocalList::const_iterator i = list.begin(); i != list.end(); ++i)
			{
				LocalUser* luser = *i;

				// Don't send to unregistered users or the user who is the source.
				if (luser->registered != REG_ALL || luser == source)
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
				target = ServerInstance->FindNickOnly(parameters[0].substr(0, targetserver - parameters[0].c_str()));
				if (target && strcasecmp(target->server->GetName().c_str(), targetserver + 1))
					target = NULL;
			}
			else
			{
				// If the source is a local user then we only look up the target by nick.
				target = ServerInstance->FindNickOnly(parameters[0]);
			}
		}
		else
		{
			// Remote users can only specify a nick or UUID as the target.
			target = ServerInstance->FindNick(parameters[0]);
		}

		if (!target || target->registered != REG_ALL)
		{
			// The target user does not exist or is not fully registered.
			source->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
			return CMD_FAILURE;
		}

		// If the target is away then inform the user.
		if (target->IsAway() && msgtype == MSG_PRIVMSG)
			source->WriteNumeric(RPL_AWAY, target->nick, target->awaymsg);

		// Fire the pre-message events.
		MessageTarget msgtarget(target);
		MessageDetailsImpl msgdetails(msgtype, parameters[1], parameters.GetTags());
		if (!FirePreEvents(source, msgtarget, msgdetails))
			return CMD_FAILURE;

		LocalUser* const localtarget = IS_LOCAL(target);
		if (localtarget)
		{
			// Send to the target if they are a local user.
			ClientProtocol::Messages::Privmsg privmsg(ClientProtocol::Messages::Privmsg::nocopy, source, localtarget->nick, msgdetails.text, msgtype);
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
		, moderatedmode(parent, "moderated")
		, noextmsgmode(parent, "noextmsg")
	{
		syntax = "<target>[,<target>]+ :<message>";
	}

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		if (CommandParser::LoopCall(user, this, parameters, 0))
			return CMD_SUCCESS;

		// The specified message was empty.
		if (parameters[1].empty())
		{
			user->WriteNumeric(ERR_NOTEXTTOSEND, "No text to send");
			return CMD_FAILURE;
		}

		// The target is a server glob.
		if (parameters[0][0] == '$')
			return HandleServerTarget(user, parameters);

		// If the message begins with a status character then look it up.
		const char* target = parameters[0].c_str();
		PrefixMode* pmh = ServerInstance->Modes->FindPrefix(target[0]);
		if (pmh)
			target++;

		// The target is a channel name.
		if (*target == '#')
			return HandleChannelTarget(user, parameters, target, pmh);

		// The target is a nickname.
		return HandleUserTarget(user, parameters);
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		if (IS_LOCAL(user))
			// This is handled by the OnUserPostMessage hook to split the LoopCall pieces
			return ROUTE_LOCALONLY;
		else
			return ROUTE_MESSAGE(parameters[0]);
	}
};

class CommandSQuery : public SplitCommand
{
 public:
	CommandSQuery(Module* Creator)
		: SplitCommand(Creator, "SQUERY", 2, 2)
	{
		syntax = "<service> :<message>";
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) CXX11_OVERRIDE
	{
		// The specified message was empty.
		if (parameters[1].empty())
		{
			user->WriteNumeric(ERR_NOTEXTTOSEND, "No text to send");
			return CMD_FAILURE;
		}

		// The target can be either a nick or a nick@server mask.
		User* target;
		const char* targetserver = strchr(parameters[0].c_str(), '@');
		if (targetserver)
		{
			// The target is a user on a specific server (e.g. jto@tolsun.oulu.fi).
			target = ServerInstance->FindNickOnly(parameters[0].substr(0, targetserver - parameters[0].c_str()));
			if (target && strcasecmp(target->server->GetName().c_str(), targetserver + 1))
				target = NULL;
		}
		else
		{
			// The targer can be on any server.
			target = ServerInstance->FindNickOnly(parameters[0]);
		}

		if (!target || target->registered != REG_ALL || !target->server->IsULine())
		{
			// The target user does not exist, is not fully registered, or is not a service.
			user->WriteNumeric(ERR_NOSUCHSERVICE, parameters[0], "No such service");
			return CMD_FAILURE;
		}

		// Fire the pre-message events.
		MessageTarget msgtarget(target);
		MessageDetailsImpl msgdetails(MSG_PRIVMSG, parameters[1], parameters.GetTags());
		if (!FirePreEvents(user, msgtarget, msgdetails))
			return CMD_FAILURE;

		// The SQUERY command targets a service on a U-lined server. This can never
		// be on the server local to the source so we don't need to do any routing
		// logic and can forward it as a PRIVMSG.

		// Fire the post-message event.
		return FirePostEvent(user, msgtarget, msgdetails);
	}
};

class ModuleCoreMessage : public Module
{
 private:
	CommandMessage cmdprivmsg;
	CommandMessage cmdnotice;
	CommandSQuery cmdsquery;

 public:
	ModuleCoreMessage()
		: cmdprivmsg(this, MSG_PRIVMSG)
		, cmdnotice(this, MSG_NOTICE)
		, cmdsquery(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides the NOTICE, PRIVMSG, and SQUERY commands", VF_CORE|VF_VENDOR);
	}
};

MODULE_INIT(ModuleCoreMessage)
