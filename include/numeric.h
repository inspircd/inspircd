/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2022, 2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2016 Attila Molnar <attilamolnar@hush.com>
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

// These numerics are used in multiple source files.
enum
{
	// From RFC 1459
	RPL_LUSEROP = 252,
	RPL_AWAY = 301,
	RPL_LISTSTART = 321,
	RPL_LIST = 322,
	RPL_LISTEND = 323,
	RPL_VERSION = 351,
	RPL_LINKS = 364,
	RPL_ENDOFLINKS = 365,
	ERR_NOSUCHNICK = 401,
	ERR_NOSUCHCHANNEL = 403,
	ERR_NORECIPIENT = 411,
	ERR_NOTEXTTOSEND = 412,
	ERR_UNKNOWNCOMMAND = 421,
	ERR_NONICKNAMEGIVEN = 431,
	ERR_ERRONEUSNICKNAME = 432,
	ERR_USERNOTINCHANNEL = 441,
	ERR_NOTONCHANNEL = 442,
	ERR_YOUREBANNEDCREEP = 465,
	ERR_UNKNOWNMODE = 472,
	ERR_BANNEDFROMCHAN = 474,
	ERR_BADCHANMASK = 476,
	ERR_NOPRIVILEGES = 481,
	ERR_CHANOPRIVSNEEDED = 482,

	// From RFC 2812.
	ERR_NOSUCHSERVER = 402,
	ERR_NOSUCHSERVICE = 408,
	ERR_UNAVAILRESOURCE = 437,
	ERR_BANLISTFULL = 478,
	ERR_RESTRICTED = 484,

	// From irc2?
	RPL_SAVENICK = 43,

	// From UnrealIRCd.
	ERR_CANTCHANGENICK = 447,

	// InspIRCd-specific.
	ERR_UNKNOWNSNOMASK = 501,
	ERR_CANTUNLOADMODULE = 972,
	RPL_UNLOADEDMODULE = 973,
	ERR_CANTLOADMODULE = 974,
	RPL_LOADEDMODULE = 975,
};

namespace Numeric
{
	class Numeric;
}

class Numeric::Numeric
{
	/** Numeric number
	 */
	unsigned int numeric;

	/** Parameters of the numeric
	 */
	CommandBase::Params params;

	/** Source server of the numeric, if NULL (the default) then it is the local server
	 */
	Server* sourceserver = nullptr;

public:
	/** Constructor
	 * @param num Numeric number (RPL_*, ERR_*)
	 */
	Numeric(unsigned int num)
		: numeric(num)
	{
	}

	/** Add a tag.
	 * @param tagname Raw name of the tag to use in the protocol.
	 * @param tagprov Provider of the tag.
	 * @param val Tag value. If empty no value will be sent with the tag.
	 * @param tagdata Tag provider specific data, will be passed to MessageTagProvider::ShouldSendTag(). Optional, defaults to NULL.
	 */
	Numeric& AddTag(const std::string& tagname, ClientProtocol::MessageTagProvider* tagprov, const std::string& val, void* tagdata = nullptr)
	{
		params.GetTags().emplace(tagname, ClientProtocol::MessageTagData(tagprov, val, tagdata));
		return *this;
	}

	/** Add all tags in a TagMap to the tags in this message. Existing tags will not be overwritten.
	 * @param newtags New tags to add.
	 */
	Numeric& AddTags(const ClientProtocol::TagMap& newtags)
	{
		params.GetTags().insert(newtags.begin(), newtags.end());
		return *this;
	}

	/** Converts the given arguments to a string and adds them to the numeric.
	 * @param args One or more arguments to the numeric.
	 */
	template <typename... Args>
	Numeric& push(Args&&... args)
	{
		(params.push_back(ConvToStr(args)), ...);
		return *this;
	}

	/** Formats the string with the specified arguments and adds them to the numeric.
	 * @param text A format string to format and then push.
	 * @param p One or more arguments to format the string with.
	 */
	template <typename... Args>
	Numeric& push_fmt(const char* text, Args&&... args)
	{
		push(fmt::format(fmt::runtime(text), std::forward<Args>(args)...));
		return *this;
	}

	/** Set the source server of the numeric. The source server defaults to the local server.
	 * @param server Server to set as source
	 */
	void SetServer(Server* server) { sourceserver = server; }

	/** Get the source server of the numeric
	 * @return Source server or NULL if the source is the local server
	 */
	Server* GetServer() const { return sourceserver; }

	/** Get the number of the numeric as an unsigned integer
	 * @return Numeric number as an unsigned integer
	 */
	unsigned int GetNumeric() const { return numeric; }

	/** Get the parameters of the numeric
	 * @return Parameters of the numeric as a const vector of strings
	 */
	const CommandBase::Params& GetParams() const { return params; }

	/** Get the parameters of the numeric
	 * @return Parameters of the numeric as a vector of strings
	 */
	CommandBase::Params& GetParams() { return params; }
};
