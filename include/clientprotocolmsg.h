/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018, 2020-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 Attila Molnar <attilamolnar@hush.com>
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

namespace ClientProtocol
{
	namespace Messages
	{
		class Numeric;
		class Join;
		struct Part;
		struct Kick;
		struct Quit;
		struct Nick;
		class Mode;
		struct Topic;
		class Privmsg;
		struct Invite;
		struct Ping;
		struct Pong;
		struct Error;
	}
}

/** Numeric message.
 * Doesn't have a fixed command name, it's always a 3 digit number padded with zeroes if necessary.
 * The first parameter is the target of the numeric which is almost always the nick of the user
 * the numeric will be sent to.
 */
class CoreExport ClientProtocol::Messages::Numeric
	: public ClientProtocol::Message
{
	char numericstr[4];

	void InitCommand(unsigned int number)
	{
		snprintf(numericstr, sizeof(numericstr), "%03u", number);
		SetCommand(numericstr);
	}

	void InitFromNumeric(const ::Numeric::Numeric& numeric)
	{
		AddTags(numeric.GetParams().GetTags());
		InitCommand(numeric.GetNumeric());
		for (const auto& param : numeric.GetParams())
			PushParamRef(param);
	}

public:
	/** Constructor, target is a User.
	 * @param num Numeric object to send. Must remain valid as long as this object is alive and must not be modified.
	 * @param user User to send the numeric to. May be partially connected, must remain valid as long as this object is alive.
	 */
	Numeric(const ::Numeric::Numeric& num, User* user)
		: ClientProtocol::Message(nullptr, (num.GetServer() ? num.GetServer() : ServerInstance->FakeClient->server)->GetPublicName())
	{
		if (user->connected & User::CONN_NICK)
			PushParamRef(user->nick);
		else
			PushParam("*");
		InitFromNumeric(num);
	}

	/** Constructor, target is a string.
	 * @param num Numeric object to send. Must remain valid as long as this object is alive and must not be modified.
	 * @param target Target string, must stay valid as long as this object is alive.
	 */
	Numeric(const ::Numeric::Numeric& num, const std::string& target)
		: ClientProtocol::Message(nullptr, (num.GetServer() ? num.GetServer() : ServerInstance->FakeClient->server)->GetPublicName())
	{
		PushParamRef(target);
		InitFromNumeric(num);
	}

	/** Constructor. Only the numeric number has to be specified.
	 * @param num Numeric number.
	 */
	Numeric(unsigned int num)
		: ClientProtocol::Message(nullptr, ServerInstance->Config->GetServerName())
	{
		InitCommand(num);
		PushParam("*");
	}
};

/** JOIN message.
 * Sent when a user joins a channel.
 */
class CoreExport ClientProtocol::Messages::Join
	: public ClientProtocol::Message
{
	Membership* memb;

public:
	/** Constructor. Does not populate parameters, call SetParams() before sending the message.
	 */
	Join()
		: ClientProtocol::Message("JOIN")
		, memb(nullptr)
	{
	}

	/** Constructor.
	 * @param Memb Membership of the joining user.
	 */
	Join(Membership* Memb)
		: ClientProtocol::Message("JOIN", Memb->user)
	{
		SetParams(Memb);
	}

	/** Constructor.
	 * @param Memb Membership of the joining user.
	 * @param sourcestrref Message source string, must remain valid as long as this object is alive.
	 */
	Join(Membership* Memb, const std::string& sourcestrref)
		: ClientProtocol::Message("JOIN", sourcestrref, Memb->user)
	{
		SetParams(Memb);
	}

	/** Populate parameters from a Membership
	 * @param Memb Membership of the joining user.
	 */
	void SetParams(Membership* Memb)
	{
		memb = Memb;
		PushParamRef(memb->chan->name);
	}

	/** Get the Membership of the joining user.
	 * @return Membership of the joining user.
	 */
	Membership* GetMember() const { return memb; }
};

/** PART message.
 * Sent when a user parts a channel.
 */
struct CoreExport ClientProtocol::Messages::Part
	: public ClientProtocol::Message
{
	/** Constructor.
	 * @param memb Member parting.
	 * @param reason Part reason, may be empty. If non-empty, must remain valid as long as this object is alive.
	 */
	Part(Membership* memb, const std::string& reason)
		: ClientProtocol::Message("PART", memb->user)
	{
		PushParamRef(memb->chan->name);
		if (!reason.empty())
			PushParamRef(reason);
	}
};

/** KICK message.
 * Sent when a user is kicked from a channel.
 */
struct CoreExport ClientProtocol::Messages::Kick
	: public ClientProtocol::Message
{
	/** Constructor.
	 * @param source User that does the kick.
	 * @param memb Membership of the user being kicked.
	 * @param reason Kick reason. Must remain valid as long as this object is alive.
	 */
	Kick(User* source, Membership* memb, const std::string& reason)
		: ClientProtocol::Message("KICK", source)
	{
		PushParamRef(memb->chan->name);
		PushParamRef(memb->user->nick);
		PushParamRef(reason);
	}
};

/** QUIT message.
 * Sent when a user quits.
 */
struct CoreExport ClientProtocol::Messages::Quit
	: public ClientProtocol::Message
{
	/** Constructor.
	 * @param source User quitting.
	 * @param reason Quit reason, may be empty. Must remain valid as long as this object is alive.
	 */
	Quit(User* source, const std::string& reason)
		: ClientProtocol::Message("QUIT", source)
	{
		if (!reason.empty())
			PushParamRef(reason);
	}
};

/** NICK message.
 * Sent when a user changes their nickname.
 */
struct CoreExport ClientProtocol::Messages::Nick
	: public ClientProtocol::Message
{
	/** Constructor.
	 * @param source User changing nicks.
	 * @param newnick New nickname. Must remain valid as long as this object is alive.
	 */
	Nick(User* source, const std::string& newnick)
		: ClientProtocol::Message("NICK", source)
	{
		PushParamRef(newnick);
	}
};

/** MODE message.
 * Sent when modes are changed on a user or channel.
 */
class CoreExport ClientProtocol::Messages::Mode
	: public ClientProtocol::Message
{
	Channel* chantarget;
	User* usertarget;
	Modes::ChangeList::List::const_iterator beginit;
	Modes::ChangeList::List::const_iterator lastit;

	/** Convert a range of a mode change list to mode letters and '+', '-' symbols.
	 * @param list Mode change list.
	 * @param maxlinelen Maximum output length.
	 * @param beginit Iterator to the first element in 'list' to process.
	 * @param lastit Iterator which is set to the first element not processed due to length limitations by the method.
	 */
	static std::string ToModeLetters(const Modes::ChangeList::List& list, std::string::size_type maxlinelen, Modes::ChangeList::List::const_iterator beginit, Modes::ChangeList::List::const_iterator& lastit)
	{
		std::string ret;
		std::string::size_type paramlength = 0;
		char output_pm = '\0'; // current output state, '+' or '-'

		Modes::ChangeList::List::const_iterator i;
		for (i = beginit; i != list.end(); ++i)
		{
			const Modes::Change& item = *i;

			const char needed_pm = (item.adding ? '+' : '-');
			if (needed_pm != output_pm)
			{
				output_pm = needed_pm;
				ret.push_back(output_pm);
			}

			if (!item.param.empty())
				paramlength += item.param.length() + 1;
			if (ret.length() + 1 + paramlength > maxlinelen)
			{
				// Mode sequence is getting too long
				if ((ret.back() == '+') || (ret.back() == '-'))
					ret.pop_back();
				break;
			}

			ret.push_back(item.mh->GetModeChar());
		}

		lastit = i;
		return ret;
	}

	/** Push mode parameters for modes that have one, starting at beginit to lastit (not including lastit).
	 */
	void PushModeParams()
	{
		for (Modes::ChangeList::List::const_iterator i = beginit; i != lastit; ++i)
		{
			const Modes::Change& item = *i;
			if (!item.param.empty())
				PushParamRef(item.param);
		}
	}

public:
	/** Convert an entire mode change list into mode letters and '+' and '-' characters.
	 * @param changelist Mode change list to convert into mode letters.
	 * @return Mode letters.
	 */
	static std::string ToModeLetters(const Modes::ChangeList& changelist)
	{
		std::string dummystr;
		Modes::ChangeList::List::const_iterator dummy;
		return ToModeLetters(changelist.getlist(), dummystr.max_size(), changelist.getlist().begin(), dummy);
	}

	/** Constructor, populate parameters starting from a given position in a mode change list.
	 * @param source User doing the mode change.
	 * @param Chantarget Channel target of the mode change. May be NULL if Usertarget is non-NULL.
	 * @param Usertarget User target of the mode change. May be NULL if Chantarget is non-NULL.
	 * @param changelist Mode change list. Must remain valid and unchanged as long as this object is alive or until the next SetParams() call.
	 * @param beginiter Starting position of mode changes in 'changelist'.
	 */
	Mode(User* source, Channel* Chantarget, User* Usertarget, const Modes::ChangeList& changelist, Modes::ChangeList::List::const_iterator beginiter)
		: ClientProtocol::Message("MODE", source)
		, chantarget(Chantarget)
		, usertarget(Usertarget)
		, beginit(beginiter)
	{
		PushParamRef(GetStrTarget());
		PushParam(ToModeLetters(changelist.getlist(), 450, beginit, lastit));
		PushModeParams();
	}

	/** Constructor, populate parameters starting from the beginning of a mode change list.
	 * @param source User doing the mode change.
	 * @param Chantarget Channel target of the mode change. May be NULL if Usertarget is non-NULL.
	 * @param Usertarget User target of the mode change. May be NULL if Chantarget is non-NULL.
	 * @param changelist Mode change list. Must remain valid and unchanged as long as this object is alive or until the next SetParams() call.
	 */
	Mode(User* source, Channel* Chantarget, User* Usertarget, const Modes::ChangeList& changelist)
		: ClientProtocol::Message("MODE", source)
		, chantarget(Chantarget)
		, usertarget(Usertarget)
		, beginit(changelist.getlist().begin())
	{
		PushParamRef(GetStrTarget());
		PushParam(ToModeLetters(changelist.getlist(), 450, beginit, lastit));
		PushModeParams();
	}

	/** Constructor. Does not populate parameters, call SetParams() before sending the message.
	 * The message source is set to the local server.
	 */
	Mode()
		: ClientProtocol::Message("MODE", ServerInstance->FakeClient)
		, chantarget(nullptr)
		, usertarget(nullptr)
	{
	}

	/** Set parameters
	 * @param Chantarget Channel target of the mode change. May be NULL if Usertarget is non-NULL.
	 * @param Usertarget User target of the mode change. May be NULL if Chantarget is non-NULL.
	 * @param changelist Mode change list. Must remain valid and unchanged as long as this object is alive or until the next SetParams() call.
	 */
	void SetParams(Channel* Chantarget, User* Usertarget, const Modes::ChangeList& changelist)
	{
		ClearParams();

		chantarget = Chantarget;
		usertarget = Usertarget;
		beginit = changelist.getlist().begin();

		PushParamRef(GetStrTarget());
		PushParam(ToModeLetters(changelist.getlist(), 450, beginit, lastit));
		PushModeParams();
	}

	/** Get first mode change included in this MODE message.
	 * @return Iterator to the first mode change that is included in this MODE message.
	 */
	Modes::ChangeList::List::const_iterator	GetBeginIterator() const { return beginit; }

	/** Get first mode change not included in this MODE message.
	 * @return Iterator to the first mode change that is not included in this MODE message.
	 */
	Modes::ChangeList::List::const_iterator GetEndIterator() const { return lastit; }

	/** Get mode change target as a string.
	 * This is the name of the channel if the mode change targets a channel or the nickname of the user
	 * if the target is a user.
	 * @return Name of target as a string.
	 */
	const std::string& GetStrTarget() const { return (chantarget ? chantarget->name : usertarget->nick); }

	/** Get user target.
	 * @return User target or NULL if the mode change targets a channel.
	 */
	User* GetUserTarget() const { return usertarget; }

	/** Get channel target.
	 * @return Channel target or NULL if the mode change targets a user.
	 */
	Channel* GetChanTarget() const { return chantarget; }
};

/** TOPIC message.
 */
struct CoreExport ClientProtocol::Messages::Topic
	: public ClientProtocol::Message
{
	/** Constructor.
	 * @param source User changing the topic.
	 * @param chan Channel the topic is being changed on.
	 * @param newtopic New topic. May be empty, must remain valid as long as this object is alive.
	 */
	Topic(User* source, const Channel* chan, const std::string& newtopic)
		: ClientProtocol::Message("TOPIC", source)
	{
		PushParamRef(chan->name);
		PushParamRef(newtopic);
	}
};

/** PRIVMSG and NOTICE message.
 */
class CoreExport ClientProtocol::Messages::Privmsg
	: public ClientProtocol::Message
{
	void PushTargetChan(char status, const Channel* targetchan)
	{
		if (status)
		{
			std::string rawtarget(1, status);
			rawtarget.append(targetchan->name);
			PushParam(rawtarget);
		}
		else
		{
			PushParamRef(targetchan->name);
		}
	}

	void PushTargetUser(const User* targetuser)
	{
		if (targetuser->connected & User::CONN_NICK)
			PushParamRef(targetuser->nick);
		else
			PushParam("*");
	}

public:
	/** Used to differentiate constructors that copy the text from constructors that do not.
	 */
	enum NoCopy { nocopy };

	/** Get command name from MessageType.
	 * @param mt Message type to get command name for.
	 * @return Command name for the message type.
	 */
	static const char* CommandStrFromMsgType(MessageType mt)
	{
		return ((mt == MessageType::PRIVMSG) ? "PRIVMSG" : "NOTICE");
	}

	/** Constructor, user source, string target, copies text.
	 * @param source Source user.
	 * @param target Privmsg target string.
	 * @param text Privmsg text, will be copied.
	 * @param mt Message type.
	 */
	Privmsg(User* source, const std::string& target, const std::string& text, MessageType mt = MessageType::PRIVMSG)
		: ClientProtocol::Message(CommandStrFromMsgType(mt), source)
	{
		PushParam(target);
		PushParam(text);
	}

	/** Constructor, user source, user target, copies text.
	 * @param source Source user.
	 * @param targetchan Target channel.
	 * @param text Privmsg text, will be copied.
	 * @param mt Message type.
	 * @param status Prefix character for status messages. If non-zero the message is a status message. Optional, defaults to 0.
	 */
	Privmsg(User* source, const Channel* targetchan, const std::string& text, MessageType mt = MessageType::PRIVMSG, char status = 0)
		: ClientProtocol::Message(CommandStrFromMsgType(mt), source)
	{
		PushTargetChan(status, targetchan);
		PushParam(text);
	}

	/** Constructor, user source, user target, copies text.
	 * @param source Source user.
	 * @param targetuser Target user.
	 * @param text Privmsg text, will be copied.
	 * @param mt Message type.
	 */
	Privmsg(User* source, const User* targetuser, const std::string& text, MessageType mt = MessageType::PRIVMSG)
		: ClientProtocol::Message(CommandStrFromMsgType(mt), source)
	{
		PushTargetUser(targetuser);
		PushParam(text);
	}

	/** Constructor, string source, string target, copies text.
	 * @param source Source user.
	 * @param target Target string.
	 * @param text Privmsg text, will be copied.
	 * @param mt Message type.
	 * @param status Prefix character for status messages. If non-zero the message is a status message. Optional, defaults to 0.
	 */
	Privmsg(const std::string& source, const std::string& target, const std::string& text, MessageType mt = MessageType::PRIVMSG, char status = 0)
		: ClientProtocol::Message(CommandStrFromMsgType(mt), source)
	{
		if (status)
		{
			std::string rawtarget(1, status);
			rawtarget.append(target);
			PushParam(rawtarget);
		}
		else
		{
			PushParam(target);
		}
		PushParam(text);
	}

	/** Constructor, string source, channel target, copies text.
	 * @param source Source string.
	 * @param targetchan Target channel.
	 * @param text Privmsg text, will be copied.
	 * @param status Prefix character for status messages. If non-zero the message is a status message. Optional, defaults to 0.
	 * @param mt Message type.
	 */
	Privmsg(const std::string& source, const Channel* targetchan, const std::string& text, MessageType mt = MessageType::PRIVMSG, char status = 0)
		: ClientProtocol::Message(CommandStrFromMsgType(mt), source)
	{
		PushTargetChan(status, targetchan);
		PushParam(text);
	}

	/** Constructor, string source, user target, copies text.
	 * @param source Source string.
	 * @param targetuser Target user.
	 * @param text Privmsg text, will be copied.
	 * @param mt Message type.
	 */
	Privmsg(const std::string& source, const User* targetuser, const std::string& text, MessageType mt = MessageType::PRIVMSG)
		: ClientProtocol::Message(CommandStrFromMsgType(mt), source)
	{
		PushTargetUser(targetuser);
		PushParam(text);
	}

	/** Constructor, user source, string target, copies text.
	 * @param source Source user.
	 * @param target Target string.
	 * @param text Privmsg text, will not be copied.
	 * @param mt Message type.
	 */
	Privmsg(NoCopy, User* source, const std::string& target, const std::string& text, MessageType mt = MessageType::PRIVMSG)
		: ClientProtocol::Message(CommandStrFromMsgType(mt), source)
	{
		PushParam(target);
		PushParamRef(text);
	}

	/** Constructor, user source, channel target, does not copy text.
	 * @param source Source user.
	 * @param targetchan Target channel.
	 * @param text Privmsg text, will not be copied.
	 * @param status Prefix character for status messages. If non-zero the message is a status message. Optional, defaults to 0.
	 * @param mt Message type.
	 */
	Privmsg(NoCopy, User* source, const Channel* targetchan, const std::string& text, MessageType mt = MessageType::PRIVMSG, char status = 0)
		: ClientProtocol::Message(CommandStrFromMsgType(mt), source)
	{
		PushTargetChan(status, targetchan);
		PushParamRef(text);
	}

	/** Constructor, user source, user target, does not copy text.
	 * @param source Source user.
	 * @param targetuser Target user.
	 * @param text Privmsg text, will not be copied.
	 * @param mt Message type.
	 */
	Privmsg(NoCopy, User* source, const User* targetuser, const std::string& text, MessageType mt = MessageType::PRIVMSG)
		: ClientProtocol::Message(CommandStrFromMsgType(mt), source)
	{
		PushTargetUser(targetuser);
		PushParamRef(text);
	}

	/** Constructor, string source, string target, does not copy text.
	 * @param source Source string.
	 * @param target Target string.
	 * @param text Privmsg text, will not be copied.
	 * @param mt Message type.
	 */
	Privmsg(NoCopy, const std::string& source, const std::string& target, const std::string& text, MessageType mt = MessageType::PRIVMSG)
		: ClientProtocol::Message(CommandStrFromMsgType(mt), source)
	{
		PushParam(target);
		PushParamRef(text);
	}

	/** Constructor, string source, channel target, does not copy text.
	 * @param source Source string.
	 * @param targetchan Target channel.
	 * @param text Privmsg text, will not be copied.
	 * @param status Prefix character for status messages. If non-zero the message is a status message. Optional, defaults to 0.
	 * @param mt Message type.
	 */
	Privmsg(NoCopy, const std::string& source, const Channel* targetchan, const std::string& text, MessageType mt = MessageType::PRIVMSG, char status = 0)
		: ClientProtocol::Message(CommandStrFromMsgType(mt), source)
	{
		PushTargetChan(status, targetchan);
		PushParamRef(text);
	}

	/** Constructor, string source, user target, does not copy text.
	 * @param source Source string.
	 * @param targetuser Target user.
	 * @param text Privmsg text, will not be copied.
	 * @param mt Message type.
	 */
	Privmsg(NoCopy, const std::string& source, const User* targetuser, const std::string& text, MessageType mt = MessageType::PRIVMSG)
		: ClientProtocol::Message(CommandStrFromMsgType(mt), source)
	{
		PushTargetUser(targetuser);
		PushParamRef(text);
	}
};

/** INVITE message.
 * Sent when a user is invited to join a channel.
 */
struct CoreExport ClientProtocol::Messages::Invite
	: public ClientProtocol::Message
{
	/** Constructor.
	 * @param source User inviting the target user.
	 * @param target User being invited by source.
	 * @param chan Channel the target user is being invited to.
	 */
	Invite(User* source, User* target, Channel* chan)
		: ClientProtocol::Message("INVITE", source)
	{
		PushParamRef(target->nick);
		PushParamRef(chan->name);
	}
};

/** PING message.
 * Used to check if a connection is still alive.
 */
struct CoreExport ClientProtocol::Messages::Ping
	: public ClientProtocol::Message
{
	/** Constructor.
	 * The ping cookie is the name of the local server.
	 */
	Ping()
		: ClientProtocol::Message("PING")
	{
		PushParamRef(ServerInstance->Config->GetServerName());
	}

	/** Constructor.
	 * @param cookie Ping cookie. Must remain valid as long as this object is alive.
	 */
	Ping(const std::string& cookie)
		: ClientProtocol::Message("PING")
	{
		PushParamRef(cookie);
	}
};

/** PONG message.
 * Sent as a reply to PING.
 */
struct CoreExport ClientProtocol::Messages::Pong
	: public ClientProtocol::Message
{
	/** Constructor.
	 * @param cookie Ping cookie. Must remain valid as long as this object is alive.
	 * @param server Pinged server. Must remain valid as long as this object is alive.
	 */
	Pong(const std::string& cookie, const std::string& server = "")
		: ClientProtocol::Message("PONG", ServerInstance->Config->GetServerName())
	{
		PushParamRef(server);
		PushParamRef(cookie);
	}
};

/** ERROR message.
 * Sent to clients upon disconnection.
 */
struct CoreExport ClientProtocol::Messages::Error
	: public ClientProtocol::Message
{
	/** Constructor.
	 * @param text Error text.
	 */
	Error(const std::string& text)
			: ClientProtocol::Message("ERROR")
	{
		PushParam(text);
	}
};
