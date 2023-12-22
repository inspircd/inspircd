/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2020, 2022-2023 Sadie Powell <sadie@witchery.services>
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

#include "event.h"

namespace ClientProtocol
{
	class EventHook;
	class MessageSource;
	struct RFCEvents;
	struct ParseOutput;
	class TagSelection;
}

/** Contains a message parsed from wire format.
 * Used by Serializer::Parse().
 */
struct CoreExport ClientProtocol::ParseOutput final
{
	/** Command name, must not be empty.
	 */
	std::string cmd;

	/** Parameter list, may be empty.
	 */
	ClientProtocol::ParamList params;

	/** Message tags, may be empty.
	 */
	ClientProtocol::TagMap tags;
};

/** A selection of zero or more tags in a TagMap.
 */
class CoreExport ClientProtocol::TagSelection final
{
	std::bitset<64> selection;

public:
	/** Check if a tag is selected.
	 * @param tags TagMap the tag is in. The TagMap must contain the same tags as it had when the tag
	 * was selected with Select(), otherwise the result is not meaningful.
	 * @param it Iterator to the tag to check.
	 * @return True if the tag is selected, false otherwise.
	 */
	bool IsSelected(const TagMap& tags, TagMap::const_iterator it) const
	{
		const size_t index = std::distance(tags.begin(), it);
		return ((index < selection.size()) && (selection[index]));
	}

	/** Select a tag.
	 * @param tags TagMap the tag is in. This parameter must be the same every time the method is called.
	 * The TagMap must not be altered otherwise the results of IsSelected() is not meaningful.
	 * @param it Iterator to the tag to mark as selected.
	 */
	void Select(const TagMap& tags, TagMap::const_iterator it)
	{
		const size_t index = std::distance(tags.begin(), it);
		if (index < selection.size())
			selection[index] = true;
	}

	/** Check if a TagSelection is equivalent to this object.
	 * @param other Other TagSelection object to compare this with.
	 * @return True if the objects are equivalent, false if they aren't.
	 */
	bool operator==(const TagSelection& other) const
	{
		return (this->selection == other.selection);
	}
};

class CoreExport ClientProtocol::MessageSource
{
 protected:
	User* sourceuser;
	const std::string* sourcestr;
	bool sourceowned:1;

public:
	/** Constructor, sets the source to be the full host of a user or sets it to be nothing.
	 * The actual source string when serializing will be obtained from User::GetFullHost() if the user is non-NULL.
	 * @param Sourceuser User to set as source of the message. If NULL, the message won't have a source when serialized.
	 * Optional, defaults to NULL.
	 */
	MessageSource(User* Sourceuser = nullptr)
		: sourceowned(false)
	{
		SetSourceUser(Sourceuser);
	}

	~MessageSource()
	{
		if (sourceowned && sourcestr)
			delete sourcestr;
	}

	/** Constructor, sets the source to the supplied string and optionally sets the source user.
	 * @param Sourcestr String to use as message source for serialization purposes. Must remain valid
	 * as long as this object is alive.
	 * @param Sourceuser User to set as source. Optional, defaults to NULL. It will not be used for serialization but
	 * if provided it may be used internally, for example to create message tags.
	 * Useful when the source string is synthesized but it is still related to a User.
	 */
	MessageSource(const std::string& Sourcestr, User* Sourceuser = nullptr)
		: sourceowned(false)
	{
		SetSource(Sourcestr, Sourceuser);
	}

	/** Get the source of this message as a string.
	 * @return Pointer to the message source string or NULL if there is no source.
	 */
	const std::string* GetSource() const
	{
		// Return string if there's one explicitly set
		if (sourcestr)
			return sourcestr;
		if (sourceuser)
			return &sourceuser->GetMask();
		return nullptr;
	}

	/** Get the source User.
	 * This shouldn't be used for serialization, use GetSource() for that.
	 * @return User pointer if the message has a source user, NULL otherwise.
	 */
	User* GetSourceUser() const { return sourceuser; }

	/** Set the source of this message to a User.
	 * See the one parameter constructor for a more detailed description.
	 * @param Sourceuser User to set as source.
	 */
	void SetSourceUser(User* Sourceuser)
	{
		sourceuser = Sourceuser;
		sourcestr = nullptr;
	}

	/** Set the source string and optionally source user.
	 * See the two parameter constructor for a more detailed description.
	 * @param Sourcestr String source, to be used for serialization purposes. Must remain valid as long
	 * as this object is alive.
	 * @param Sourceuser Source user to set, optional.
	 */
	void SetSource(const std::string& Sourcestr, User* Sourceuser = nullptr)
	{
		sourcestr = &Sourcestr;
		sourceuser = Sourceuser;
	}

	/** Copy the source from a MessageSource object.
	 * @param other MessageSource object to copy from.
	 */
	void SetSource(const MessageSource& other)
	{
		sourcestr = other.sourcestr;
		sourceuser = other.sourceuser;
	}
};

/** Outgoing client protocol message.
 * Represents a high level client protocol message which is turned into raw or wire format
 * by a Serializer. Messages can be serialized into different format by different serializers.
 *
 * Messages are created on demand and are disposed of after they have been sent.
 *
 * All messages have a command name, a list of parameters and a map of tags, the last two can be empty.
 * They also always have a source, see class MessageSource.
 */
class CoreExport ClientProtocol::Message
	: public ClientProtocol::MessageSource
{
public:
	/** Contains information required to identify a specific version of a serialized message.
	 */
	struct SerializedInfo final
	{
		const Serializer* serializer;
		TagSelection tagwl;

		/** Constructor.
		 * @param Ser Serializer used to serialize the message.
		 * @param Tagwl Tag whitelist used to serialize the message.
		 */
		SerializedInfo(const Serializer* Ser, const TagSelection& Tagwl)
			: serializer(Ser)
			, tagwl(Tagwl)
		{
		}

		/** Check if a SerializedInfo object is equivalent to this object.
		 * @param other Other SerializedInfo object.
		 * @return True if other is equivalent to this object, false otherwise.
		 */
		bool operator==(const SerializedInfo& other) const
		{
			return ((serializer == other.serializer) && (tagwl == other.tagwl));
		}
	};

	class Param final
	{
		const std::string* ptr;
		insp::aligned_storage<std::string> str;
		bool owned;

		void InitFrom(const Param& other)
		{
			owned = other.owned;
			if (owned)
				new(str) std::string(*other.str);
			else
				ptr = other.ptr;
		}

	public:
		operator const std::string&() const { return (owned ? *str : *ptr); }

		Param()
			: ptr(nullptr)
			, owned(false)
		{
		}

		Param(const std::string& s)
			: ptr(&s)
			, owned(false)
		{
		}

		Param(int, const char* s)
			: ptr(nullptr)
			, owned(true)
		{
			new(str) std::string(s);
		}

		Param(int, const std::string& s)
			: ptr(nullptr)
			, owned(true)
		{
			new(str) std::string(s);
		}

		Param(const Param& other)
		{
			InitFrom(other);
		}

		~Param()
		{
			using std::string;
			if (owned)
				str->~string();
		}

		Param& operator=(const Param& other)
		{
			if (&other == this)
				return *this;

			using std::string;
			if (owned)
				str->~string();
			InitFrom(other);
			return *this;
		}

		bool IsOwned() const { return owned; }
	};

	typedef std::vector<Param> ParamList;

	/** Escapes a value to the tag format.
	 * @param value The value to escape.
	 */
	static std::string EscapeTag(const std::string& value);

	/** Unescapes a value from the tag format.
	 * @param value The value to unescape.
	 */
	static std::string UnescapeTag(const std::string& value);

private:
	typedef std::vector<std::pair<SerializedInfo, SerializedMessage>> SerializedList;

	ParamList params;
	TagMap tags;
	std::string command;
	bool msginit_done = false;
	mutable SerializedList serlist;
	bool sideeffect = false;

protected:
	/** Set command string.
	 * @param cmd Command string to set.
	 */
	void SetCommand(const char* cmd)
	{
		command.clear();
		if (cmd)
			command = cmd;
	}

public:
	/** Constructor.
	 * @param cmd Command name, e.g. "JOIN", "NICK". May be NULL. If NULL, the command must be set
	 * with SetCommand() before the message is serialized.
	 * @param Sourceuser See the one parameter constructor of MessageSource for description.
	 */
	Message(const char* cmd, User* Sourceuser = nullptr)
		: ClientProtocol::MessageSource(Sourceuser)
		, command(cmd ? cmd : std::string())
	{
		params.reserve(8);
		serlist.reserve(8);
	}

	/** Constructor.
	 * @param cmd Command name, e.g. "JOIN", "NICK". May be NULL. If NULL, the command must be set
	 * with SetCommand() before the message is serialized.
	 * @param Sourcestr See the two parameter constructor of MessageSource for description.
	 * Must remain valid as long as this object is alive.
	 * @param Sourceuser See the two parameter constructor of MessageSource for description.
	 */
	Message(const char* cmd, const std::string& Sourcestr, User* Sourceuser = nullptr)
		: ClientProtocol::MessageSource(Sourcestr, Sourceuser)
		, command(cmd ? cmd : std::string())
	{
		params.reserve(8);
		serlist.reserve(8);
	}

	/** Get the parameters of this message.
	 * @return List of parameters.
	 */
	const ParamList& GetParams() const { return params; }

	/** Get a map of tags attached to this message.
	 * The map contains the tag providers that attached the tag to the message.
	 * @return Map of tags.
	 */
	const TagMap& GetTags() const { return tags; }

	/** Get the command string.
	 * @return Command string, e.g. "NICK", "001".
	 */
	const char* GetCommand() const { return command.c_str(); }

	/** Add a parameter to the parameter list.
	 * @param str String to add, will be copied.
	 */
	void PushParam(const char* str) { params.emplace_back(0, str); }

	/** Add a parameter to the parameter list.
	 * @param str String to add, will be copied.
	 */
	void PushParam(const std::string& str) { params.emplace_back(0, str); }

	/** Converts the given arguments to a string and adds them to the parameter list.
	 * @param args One or more parameters to add to the parameter list.
	 */
	template <typename... Args>
	void PushParam(Args&&... args)
	{
		(PushParam(ConvToStr(args)), ...);
	}

	/** Add a parameter to the parameter list.
	 * @param str String to add.
	 * The string will NOT be copied, it must remain alive until ClearParams() is called or until the object is destroyed.
	 */
	void PushParamRef(const std::string& str) { params.push_back(str); }

	/** Add a placeholder parameter to the parameter list.
	 * Placeholder parameters must be filled in later with actual parameters using ReplaceParam() or ReplaceParamRef().
	 */
	void PushParamPlaceholder() { params.emplace_back(); }

	/** Replace a parameter or a placeholder that is already in the parameter list.
	 * @param index Index of the parameter to replace. Must be less than GetParams().size().
	 * @param str String to replace the parameter or placeholder with, will be copied.
	 */
	void ReplaceParam(size_t index, const char* str) { params[index] = Param(0, str); }

	/** Replace a parameter or a placeholder that is already in the parameter list.
	 * @param index Index of the parameter to replace. Must be less than GetParams().size().
	 * @param str String to replace the parameter or placeholder with, will be copied.
	 */
	void ReplaceParam(size_t index, const std::string& str) { params[index] = Param(0, str); }

	/** Replace a parameter or a placeholder that is already in the parameter list.
	 * @param index Index of the parameter to replace. Must be less than GetParams().size().
	 * @param str String to replace the parameter or placeholder with.
	 * The string will NOT be copied, it must remain alive until ClearParams() is called or until the object is destroyed.
	 */
	void ReplaceParamRef(size_t index, const std::string& str) { params[index] = Param(str); }

	/** Add a tag.
	 * @param tagname Raw name of the tag to use in the protocol.
	 * @param tagprov Provider of the tag.
	 * @param val Tag value. If empty no value will be sent with the tag.
	 * @param tagdata Tag provider specific data, will be passed to MessageTagProvider::ShouldSendTag(). Optional, defaults to NULL.
	 */
	void AddTag(const std::string& tagname, MessageTagProvider* tagprov, const std::string& val, void* tagdata = nullptr)
	{
		tags.emplace(tagname, MessageTagData(tagprov, val, tagdata));
	}

	/** Add all tags in a TagMap to the tags in this message. Existing tags will not be overwritten.
	 * @param newtags New tags to add.
	 */
	void AddTags(const ClientProtocol::TagMap& newtags)
	{
		tags.insert(newtags.begin(), newtags.end());
	}

	/** Get the message in a serialized form.
	 * @param serializeinfo Information about which exact serialized form of the message is the caller asking for
	 * (which serializer to use and which tags to include).
	 * @return Serialized message according to serializeinfo. The returned reference remains valid until the
	 * next call to this method.
	 */
	const SerializedMessage& GetSerialized(const SerializedInfo& serializeinfo) const;

	/** Clear the parameter list and tags.
	 */
	void ClearParams()
	{
		msginit_done = false;
		params.clear();
		tags.clear();
		InvalidateCache();
	}

	/** Remove all serialized messages.
	 * If a parameter is changed after the message has been sent at least once, this method must be called before
	 * serializing the message again to ensure the cache won't contain stale data.
	 */
	void InvalidateCache()
	{
		serlist.clear();
	}

	void CopyAll()
	{
		size_t idx = 0;
		for (const auto& param : params)
		{
			if (!param.IsOwned())
				ReplaceParam(idx, param);
			idx++;
		}

		if (GetSource())
		{
			sourcestr = new std::string(*GetSource());
			sourceowned = true;
		}
	}

	void SetSideEffect(bool Sideeffect) { sideeffect = Sideeffect; }
	bool IsSideEffect() const { return sideeffect; }

	friend class Serializer;
};

/** Client protocol event class.
 * All messages sent to a user must be part of an event. A single event may result in more than one protocol message
 * being sent, for example a join event may result in a JOIN and a MODE protocol message sent to members of the channel
 * if the joining user has some prefix modes set.
 *
 * Event hooks attached to a specific event can alter the messages sent for that event.
 */
class CoreExport ClientProtocol::Event
{
	EventProvider* event;
	Message* initialmsg = nullptr;
	const MessageList* initialmsglist = nullptr;
	bool eventinit_done = false;

public:
	/** Constructor.
	 * @param protoeventprov Protocol event provider the event is an instance of.
	 */
	Event(EventProvider& protoeventprov)
		: event(&protoeventprov)
	{
	}

	/** Constructor.
	 * @param protoeventprov Protocol event provider the event is an instance of.
	 * @param msg Message to include in this event by default.
	 */
	Event(EventProvider& protoeventprov, ClientProtocol::Message& msg)
		: event(&protoeventprov)
		, initialmsg(&msg)
	{
	}

	/** Set a single message as the initial message in the event.
	 * Modules may alter this later.
	 */
	void SetMessage(Message* msg)
	{
		initialmsg = msg;
		initialmsglist = nullptr;
	}

	/** Set a list of messages as the initial messages in the event.
	 * Modules may alter this later.
	 */
	void SetMessageList(const MessageList& msglist)
	{
		initialmsg = nullptr;
		initialmsglist = &msglist;
	}

	/** Get a list of messages to send to a user.
	 * The exact messages sent to a user are determined by the initial message(s) set and hooks.
	 * @param user User to get the messages for.
	 * @param messagelist List to fill in with messages to send to the user for the event
	 */
	void GetMessagesForUser(LocalUser* user, MessageList& messagelist);
};

class CoreExport ClientProtocol::MessageTagEvent
	: public Events::ModuleEventProvider
{
public:
	MessageTagEvent(Module* mod)
		: ModuleEventProvider(mod, "event/messagetag")
	{
	}
};

/** Base class for message tag providers.
 * All message tags belong to a message tag provider. Message tag providers can populate messages
 * with tags before the message is sent and they have the job of determining whether a user should
 * get a message tag or be allowed to send one.
 */
class CoreExport ClientProtocol::MessageTagProvider
	: public Events::ModuleEventListener
{
public:
	/** Constructor.
	 * @param mod Module owning the provider.
	 * @param eventprio The priority to give this event listener.
	 */
	MessageTagProvider(Module* mod, unsigned int eventprio = DefaultPriority)
		: Events::ModuleEventListener(mod, "event/messagetag", eventprio)
	{
	}

	/** Called when a message is ready to be sent to give the tag provider a chance to add tags to the message.
	 * To add tags call Message::AddTag(). If the provided tag or tags have been added already elsewhere or if the
	 * provider doesn't want its tag(s) to be on the message, the implementation doesn't have to do anything special.
	 * The default implementation does nothing.
	 * @param msg Message to be populated with tags.
	 */
	virtual void OnPopulateTags(ClientProtocol::Message& msg)
	{
	}

	/** Called for each tag that the server receives from a client in a message.
	 * @param user User that sent the tag.
	 * @param tagname Name of the tag.
	 * @param tagvalue Value of the tag, empty string if the tag has no value. May be modified.
	 * @return MOD_RES_ALLOW to accept the tag with the value in 'value', MOD_RES_DENY to reject the tag and act as if it wasn't sent,
	 * MOD_RES_PASSTHRU to make no decision. If no hooks accept a tag, the tag is rejected.
	 * The default implementation returns MOD_RES_PASSTHRU.
	 */
	virtual ModResult OnProcessTag(User* user, const std::string& tagname, std::string& tagvalue)
	{
		return MOD_RES_PASSTHRU;
	}

	/** Called whenever a user is about to receive a message that has a tag attached which is provided by this provider
	 * to determine whether or not the user should get the tag.
	 * @param user User in question.
	 * @param tagdata Tag in question.
	 * @return True if the tag should be sent to the user, false otherwise.
	 */
	virtual bool ShouldSendTag(LocalUser* user, const MessageTagData& tagdata) = 0;
};

/** Base class for client protocol event hooks.
 * A protocol event hook is attached to a single event type. It has the ability to alter or block messages
 * sent to users which belong to the event the hook is attached to.
 */
class CoreExport ClientProtocol::EventHook
	: public Events::ModuleEventListener
{
public:
	static std::string GetEventName(const std::string& name)
	{
		return "event/protoevent_" + name;
	}

	/** Constructor.
	 * @param mod Owner of the hook.
	 * @param name Name of the event to hook.
	 * @param priority Priority of the hook. Determines the order in which hooks for the same event get called.
	 * Optional.
	 */
	EventHook(Module* mod, const std::string& name, unsigned int priority = Events::ModuleEventListener::DefaultPriority)
		: Events::ModuleEventListener(mod, GetEventName(name), priority)
	{
	}

	/** Called exactly once before an event is sent to any user.
	 * The default implementation doesn't do anything.
	 * @param ev Event being sent to one or more users.
	 */
	virtual void OnEventInit(const ClientProtocol::Event& ev)
	{
	}

	/** Called for each user that may receive the event.
	 * The hook may alter the messages sent to the user and decide whether further hooks should be executed.
	 * @param user User the message list is being prepared to be sent to.
	 * @param ev Event associated with the messages.
	 * @param messagelist List of messages to send to the user. The hook can alter this in any way it sees fit, for example it can replace messages,
	 * add new messages, etc. The list must not be empty when the method returns.
	 * @return MOD_RES_PASSTHRU to run hooks after the called hook or if no hooks are after the called hook, send the messages in messagelist to the user.
	 * MOD_RES_DENY to not send any messages to the user and to not run other hooks,
	 * MOD_RES_ALLOW to send the messages in messagelist to the user and to not run other hooks.
	 */
	virtual ModResult OnPreEventSend(LocalUser* user, const ClientProtocol::Event& ev, ClientProtocol::MessageList& messagelist) = 0;
};

/** Event provider for client protocol events.
 * Protocol event hooks can be attached to the instances of these providers. The core has event
 * providers for most common IRC events defined in RFC1459.
 */
class CoreExport ClientProtocol::EventProvider final
	: public Events::ModuleEventProvider
{
public:
	/** Constructor.
	 * @param Mod Module that owns the event provider.
	 * @param eventname Name of the event this provider is for, e.g. "JOIN", "PART", "NUMERIC".
	 * Should match command name if applicable.
	 */
	EventProvider(Module* Mod, const std::string& eventname)
		: Events::ModuleEventProvider(Mod, ClientProtocol::EventHook::GetEventName(eventname))
	{
	}
};

/** Commonly used client protocol events.
 * Available via InspIRCd::GetRFCEvents().
 */
struct CoreExport ClientProtocol::RFCEvents final
{
	EventProvider numeric;
	EventProvider join;
	EventProvider part;
	EventProvider kick;
	EventProvider quit;
	EventProvider nick;
	EventProvider mode;
	EventProvider topic;
	EventProvider privmsg;
	EventProvider invite;
	EventProvider ping;
	EventProvider pong;
	EventProvider error;

	RFCEvents()
		: numeric(nullptr, "NUMERIC")
		, join(nullptr, "JOIN")
		, part(nullptr, "PART")
		, kick(nullptr, "KICK")
		, quit(nullptr, "QUIT")
		, nick(nullptr, "NICK")
		, mode(nullptr, "MODE")
		, topic(nullptr, "TOPIC")
		, privmsg(nullptr, "PRIVMSG")
		, invite(nullptr, "INVITE")
		, ping(nullptr, "PING")
		, pong(nullptr, "PONG")
		, error(nullptr, "ERROR")
	{
	}
};

/** Base class for client protocol serializers.
 * A serializer has to implement serialization and parsing of protocol messages to/from wire format.
 */
class CoreExport ClientProtocol::Serializer
	: public DataProvider
{
private:
	ClientProtocol::MessageTagEvent evprov;

	/** Make a white list containing which tags a user should get.
	 * @param user User in question.
	 * @param tagmap Tag map that contains all possible tags.
	 * @return Whitelist of tags to send to the user.
	 */
	static TagSelection MakeTagWhitelist(LocalUser* user, const TagMap& tagmap);

public:
	/** Constructor.
	 * @param mod Module owning the serializer.
	 * @param Name Name of the serializer, e.g. "rfc".
	 */
	Serializer(Module* mod, const std::string& Name);

	/** Handle a tag in a message being parsed. Call this method for each parsed tag.
	 * @param user User sending the tag.
	 * @param tagname Name of the tag.
	 * @param tagvalue Tag value, may be empty.
	 * @param tags TagMap to place the tag into, if it gets accepted.
	 * @return True if no error occurred, false if the tag name is invalid or if this tag already exists.
	 */
	bool HandleTag(LocalUser* user, const std::string& tagname, std::string& tagvalue, TagMap& tags) const;

	/** Serialize a message for a user.
	 * @param user User to serialize the message for.
	 * @param msg Message to serialize.
	 * @return Raw serialized message, only containing the appropriate tags for the user.
	 * The reference is guaranteed to be valid as long as the Message object is alive and until the same
	 * Message is serialized for another user.
	 */
	const SerializedMessage& SerializeForUser(LocalUser* user, Message& msg);

	/** Serialize a high level protocol message into wire format.
	 * @param msg High level message to serialize. Contains all necessary information about the message, including all possible tags.
	 * @param tagwl Message tags to include in the serialized message. Tags attached to the message but not included in the whitelist must not
	 * appear in the output. This is because each user may get a different set of tags for the same message.
	 * @return Protocol message in wire format. Must contain message delimiter as well, if any (e.g. CRLF for RFC1459).
	 */
	virtual std::string Serialize(const Message& msg, const TagSelection& tagwl) const = 0;

	/** Parse a protocol message from wire format.
	 * @param user Source of the message.
	 * @param line Raw protocol message.
	 * @param parseoutput Output of the parser.
	 * @return True if the message was parsed successfully into parseoutput and should be processed, false to drop the message.
	 */
	virtual bool Parse(LocalUser* user, const std::string& line, ParseOutput& parseoutput) = 0;
};

inline ClientProtocol::MessageTagData::MessageTagData(MessageTagProvider* prov, const std::string& val, void* data)
	: tagprov(prov)
	, value(val)
	, provdata(data)
{
}
