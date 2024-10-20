/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2019 iwalkalone <iwalkalone69@gmail.com>
 *   Copyright (C) 2018-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2015 Renegade334 <contact.caaeed4f@renegade334.me.uk>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006-2007 Dennis Friis <peavey@inspircd.org>
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
#include "modules/ctctags.h"
#include "modules/isupport.h"

enum
{
	// From ircu?
	RPL_SILELIST = 271,
	RPL_ENDOFSILELIST = 272,
	ERR_SILELISTFULL = 511,

	// InspIRCd-specific.
	ERR_SILENCE = 952
};

class SilenceEntry final
{
public:
	enum SilenceFlags
	{
		// Does nothing; for internal use only.
		SF_NONE = 0,

		// Exclude users who match this flags ("x").
		SF_EXEMPT = 1,

		// Hide the contents of the message when sent to the silencer ("H").
		SF_HIDE_SILENCER = 2,

		// 4, 8, 16 are reserved for future use.

		// Matches a NOTICE targeted at a channel ("n").
		SF_NOTICE_CHANNEL = 32,

		// Matches a NOTICE targeted at a user ("N").
		SF_NOTICE_USER = 64,

		// Matches a PRIVMSG targeted at a channel ("p").
		SF_PRIVMSG_CHANNEL = 128,

		// Matches a PRIVMSG targeted at a user ("P").
		SF_PRIVMSG_USER = 256,

		// Matches a TAGMSG targeted at a channel ("t").
		SF_TAGMSG_CHANNEL = 512,

		// Matches a TAGMSG targeted at a user ("T").
		SF_TAGMSG_USER = 1024,

		// Matches a CTCP targeted at a channel ("c").
		SF_CTCP_CHANNEL = 2048,

		// Matches a CTCP targeted at a user ("C").
		SF_CTCP_USER = 4096,

		// Matches an invite to a channel ("i").
		SF_INVITE = 8192,

		// The default if no flags have been specified.
		SF_DEFAULT = SF_NOTICE_CHANNEL | SF_NOTICE_USER | SF_PRIVMSG_CHANNEL | SF_PRIVMSG_USER | SF_TAGMSG_CHANNEL |
			SF_TAGMSG_USER | SF_CTCP_CHANNEL | SF_CTCP_USER | SF_INVITE
	};

	// The flags that this mask is silenced for.
	uint32_t flags;

	// The mask which is silenced (e.g. *!*@example.com).
	std::string mask;

	SilenceEntry(uint32_t Flags, const std::string& Mask)
		: flags(Flags)
		, mask(Mask)
	{
	}

	bool operator <(const SilenceEntry& other) const
	{
		if (flags & SF_EXEMPT && other.flags & ~SF_EXEMPT)
			return true;
		if (other.flags & SF_EXEMPT && flags & ~SF_EXEMPT)
			return false;
		if (flags < other.flags)
			return true;
		if (other.flags < flags)
			return false;
		return mask < other.mask;
	}

	// Converts a flag list to a bitmask.
	static bool FlagsToBits(const std::string& flags, uint32_t& out, bool strict)
	{
		out = SF_NONE;
		for (const auto flag : flags)
		{
			switch (flag)
			{
				case 'C':
					out |= SF_CTCP_USER;
					break;
				case 'c':
					out |= SF_CTCP_CHANNEL;
					break;
				case 'd':
					out |= SF_DEFAULT;
					break;
				case 'H':
					out |= SF_HIDE_SILENCER;
					break;
				case 'i':
					out |= SF_INVITE;
					break;
				case 'N':
					out |= SF_NOTICE_USER;
					break;
				case 'n':
					out |= SF_NOTICE_CHANNEL;
					break;
				case 'P':
					out |= SF_PRIVMSG_USER;
					break;
				case 'p':
					out |= SF_PRIVMSG_CHANNEL;
					break;
				case 'T':
					out |= SF_TAGMSG_USER;
					break;
				case 't':
					out |= SF_TAGMSG_CHANNEL;
					break;
				case 'x':
					out |= SF_EXEMPT;
					break;
				default:
					if (!strict)
						continue;
					out = SF_NONE;
					return false;
			}
		}
		return true;
	}

	// Converts a bitmask to a flag list.
	static std::string BitsToFlags(uint32_t flags)
	{
		std::string out;
		if (flags & SF_CTCP_USER)
			out.push_back('C');
		if (flags & SF_CTCP_CHANNEL)
			out.push_back('c');
		if (flags & SF_HIDE_SILENCER)
			out.push_back('H');
		if (flags & SF_INVITE)
			out.push_back('i');
		if (flags & SF_NOTICE_USER)
			out.push_back('N');
		if (flags & SF_NOTICE_CHANNEL)
			out.push_back('n');
		if (flags & SF_PRIVMSG_USER)
			out.push_back('P');
		if (flags & SF_PRIVMSG_CHANNEL)
			out.push_back('p');
		if (flags & SF_TAGMSG_USER)
			out.push_back('T');
		if (flags & SF_TAGMSG_CHANNEL)
			out.push_back('t');
		if (flags & SF_EXEMPT)
			out.push_back('x');
		return out;
	}
};

typedef insp::flat_set<SilenceEntry> SilenceList;

class SilenceExtItem final
	: public SimpleExtItem<SilenceList>
{
public:
	unsigned long maxsilence;

	SilenceExtItem(Module* Creator)
		: SimpleExtItem<SilenceList>(Creator, "silence-list", ExtensionType::USER)
	{
	}

	void FromInternal(Extensible* container, const std::string& value) noexcept override
	{
		if (container->extype != this->extype)
			return;

		LocalUser* user = IS_LOCAL(static_cast<User*>(container));
		if (!user)
			return;

		// Remove the old list and create a new one.
		Unset(user, false);
		auto* list = new SilenceList();

		irc::spacesepstream ts(value);
		while (!ts.StreamEnd())
		{
			// Check we have space for another entry.
			if (list->size() >= maxsilence)
			{
				ServerInstance->Logs.Debug(MODNAME, "Oversized silence list received for {}: {}",
					user->uuid, value);
				delete list;
				return;
			}

			// Extract the mask and the flags.
			std::string mask;
			std::string flagstr;
			if (!ts.GetToken(mask) || !ts.GetToken(flagstr))
			{
				ServerInstance->Logs.Debug(MODNAME, "Malformed silence list received for {}: {}",
					user->uuid, value);
				delete list;
				return;
			}

			// Try to parse the flags.
			uint32_t flags;
			if (!SilenceEntry::FlagsToBits(flagstr, flags, false))
			{
				ServerInstance->Logs.Debug(MODNAME, "Malformed silence flags received for {}: {}",
					user->uuid, flagstr);
				delete list;
				return;
			}

			// Store the silence entry.
			list->emplace(flags, mask);
		}

		// If we have an empty list then don't store it.
		if (list->empty())
		{
			delete list;
			return;
		}

		// The value was well formed.
		Set(user, list, false);
	}

	std::string ToInternal(const Extensible* container, void* item) const noexcept override
	{
		SilenceList* list = static_cast<SilenceList*>(item);
		std::string buf;
		for (SilenceList::const_iterator iter = list->begin(); iter != list->end(); ++iter)
		{
			if (iter != list->begin())
				buf.push_back(' ');

			buf.append(iter->mask);
			buf.push_back(' ');
			buf.append(SilenceEntry::BitsToFlags(iter->flags));
		}
		return buf;
	}
};

class SilenceMessage final
	: public ClientProtocol::Message
{
public:
	SilenceMessage(const std::string& mask, const std::string& flags)
		: ClientProtocol::Message("SILENCE")
	{
		PushParam(mask);
		PushParam(flags);
	}
};

class CommandSilence final
	: public SplitCommand
{
private:
	ClientProtocol::EventProvider msgprov;

	CmdResult AddSilence(LocalUser* user, const std::string& mask, uint32_t flags)
	{
		SilenceList* list = ext.Get(user);
		if (list && list->size() > ext.maxsilence)
		{
			user->WriteNumeric(ERR_SILELISTFULL, mask, SilenceEntry::BitsToFlags(flags), "Your SILENCE list is full");
			return CmdResult::FAILURE;
		}
		else if (!list)
		{
			// There is no list; create it.
			list = new SilenceList();
			ext.Set(user, list);
		}

		if (!list->emplace(flags, mask).second)
		{
			user->WriteNumeric(ERR_SILENCE, mask, SilenceEntry::BitsToFlags(flags), "The SILENCE entry you specified already exists");
			return CmdResult::FAILURE;
		}

		SilenceMessage msg("+" + mask, SilenceEntry::BitsToFlags(flags));
		user->Send(msgprov, msg);
		return CmdResult::SUCCESS;
	}

	CmdResult RemoveSilence(LocalUser* user, const std::string& mask, uint32_t flags)
	{
		SilenceList* list = ext.Get(user);
		if (list)
		{
			for (SilenceList::iterator iter = list->begin(); iter != list->end(); ++iter)
			{
				if (!irc::equals(iter->mask, mask) || iter->flags != flags)
					continue;

				list->erase(iter);
				SilenceMessage msg("-" + mask, SilenceEntry::BitsToFlags(flags));
				user->Send(msgprov, msg);
				return CmdResult::SUCCESS;
			}
		}

		user->WriteNumeric(ERR_SILENCE, mask, SilenceEntry::BitsToFlags(flags), "The SILENCE entry you specified could not be found");
		return CmdResult::FAILURE;
	}

	CmdResult ShowSilenceList(LocalUser* user)
	{
		SilenceList* list = ext.Get(user);
		if (list)
		{
			for (const auto& entry : *list)
				user->WriteNumeric(RPL_SILELIST, entry.mask, SilenceEntry::BitsToFlags(entry.flags));
		}
		user->WriteNumeric(RPL_ENDOFSILELIST, "End of SILENCE list");
		return CmdResult::SUCCESS;
	}

public:
	SilenceExtItem ext;

	CommandSilence(Module* Creator)
		: SplitCommand(Creator, "SILENCE")
		, msgprov(Creator, "SILENCE")
		, ext(Creator)
	{
		syntax = { "[(+|-)<mask> [CcdiNnPpTtx]]" };
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		if (parameters.empty())
			return ShowSilenceList(user);

		// If neither add nor remove are specified we default to add.
		bool is_remove = parameters[0][0] == '-';

		// If a prefix mask has been given then strip it and clean it up.
		std::string mask = parameters[0];
		if (mask[0] == '-' || mask[0] == '+')
		{
			mask.erase(0, 1);
			if (mask.empty())
				mask.assign("*");
			ModeParser::CleanMask(mask);
		}

		// If the user specified a flags then use that. Otherwise, default to blocking
		// all CTCPs, invites, notices, privmsgs, and invites.
		uint32_t flags = SilenceEntry::SF_DEFAULT;
		if (parameters.size() > 1)
		{
			if (!SilenceEntry::FlagsToBits(parameters[1], flags, true))
			{
				user->WriteNumeric(ERR_SILENCE, mask, parameters[1], "You specified one or more invalid SILENCE flags");
				return CmdResult::FAILURE;
			}
			else if (flags == SilenceEntry::SF_EXEMPT)
			{
				// The user specified "x" with no other flags which does not make sense; add the "d" flag.
				flags |= SilenceEntry::SF_DEFAULT;
			}
		}

		return is_remove ? RemoveSilence(user, mask, flags) : AddSilence(user, mask, flags);
	}
};

class ModuleSilence final
	: public Module
	, public CTCTags::EventListener
	, public ISupport::EventListener
{
private:
	bool exemptservice;
	CommandSilence cmd;

	void BuildChannelExempts(User* source, Channel* channel, SilenceEntry::SilenceFlags flag, CUList& exemptions, CUList& hides)
	{
		for (const auto& [user, _] : channel->GetUsers())
		{
			uint32_t flags;
			if (!CanReceiveMessage(source, user, flag, &flags))
			{
				exemptions.insert(user);
				if (user != source && flags & SilenceEntry::SF_HIDE_SILENCER)
					hides.insert(user);
			}
		}
	}

	bool CanReceiveMessage(User* source, User* target, SilenceEntry::SilenceFlags flag, uint32_t* flags = nullptr)
	{
		// Servers handle their own clients.
		if (!IS_LOCAL(target))
			return true;

		if (exemptservice && source->server->IsService())
			return true;

		SilenceList* list = cmd.ext.Get(target);
		if (!list)
			return true;

		for (const auto& entry : *list)
		{
			if (!(entry.flags & flag))
				continue;

			if (InspIRCd::Match(source->GetMask(), entry.mask))
			{
				if (flags)
					*flags = entry.flags;

				return entry.flags & SilenceEntry::SF_EXEMPT;
			}
		}

		return true;
	}

	void HideMessage(std::string& message)
	{
		InspIRCd::StripColor(message);
		message.insert(0, "\x1DSilenced\x1D: \00315,15");
	}

public:
	ModuleSilence()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds the /SILENCE command which allows users to ignore other users on server-side.")
		, CTCTags::EventListener(this)
		, ISupport::EventListener(this)
		, cmd(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("silence");
		exemptservice = tag->getBool("exemptservice", tag->getBool("exemptservice", true));
		cmd.ext.maxsilence = tag->getNum<unsigned long>("maxentries", 32, 1);
	}

	void OnBuildISupport(ISupport::TokenMap& tokens) override
	{
		tokens["ESILENCE"] = "CcdiNnPpTtx";
		tokens["SILENCE"] = ConvToStr(cmd.ext.maxsilence);
	}

	ModResult OnUserPreInvite(User* source, User* dest, Channel* channel, time_t timeout) override
	{
		return CanReceiveMessage(source, dest, SilenceEntry::SF_INVITE) ? MOD_RES_PASSTHRU : MOD_RES_DENY;
	}

	ModResult OnUserPreMessage(User* user, MessageTarget& target, MessageDetails& details) override
	{
		std::string_view ctcpname;
		bool is_ctcp = details.IsCTCP(ctcpname) && !irc::equals(ctcpname, "ACTION");

		SilenceEntry::SilenceFlags flag = SilenceEntry::SF_NONE;
		switch (target.type)
		{
			case MessageTarget::TYPE_CHANNEL:
			{
				if (is_ctcp)
					flag = SilenceEntry::SF_CTCP_CHANNEL;
				else if (details.type == MessageType::NOTICE)
					flag = SilenceEntry::SF_NOTICE_CHANNEL;
				else if (details.type == MessageType::PRIVMSG)
					flag = SilenceEntry::SF_PRIVMSG_CHANNEL;

				CUList hides;
				BuildChannelExempts(user, target.Get<Channel>(), flag, details.exemptions, hides);
				if (!hides.empty())
				{
					// In this mode we just strip formatting and hide the message.
					std::string message = details.text;
					HideMessage(message);

					// Servers handle their own users so this cast will always work.
					ClientProtocol::Messages::Privmsg msg(user, target.Get<Channel>(), message, details.type, target.status);
					for (auto* hideuser : hides)
						static_cast<LocalUser*>(hideuser)->Send(ServerInstance->GetRFCEvents().privmsg, msg);
				}
				return MOD_RES_PASSTHRU;
			}
			case MessageTarget::TYPE_USER:
			{
				if (is_ctcp)
					flag = SilenceEntry::SF_CTCP_USER;
				else if (details.type == MessageType::NOTICE)
					flag = SilenceEntry::SF_NOTICE_USER;
				else if (details.type == MessageType::PRIVMSG)
					flag = SilenceEntry::SF_PRIVMSG_USER;

				uint32_t flags;
				if (!CanReceiveMessage(user, target.Get<User>(), flag, &flags))
				{
					details.echo_original = true;
					if (flags & SilenceEntry::SF_HIDE_SILENCER)
					{
						// In this mode we just strip formatting and hide the message.
						HideMessage(details.text);
						break;
					}
					return MOD_RES_DENY;
				}
				break;
			}
			case MessageTarget::TYPE_SERVER:
				break;
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreTagMessage(User* user, MessageTarget& target, CTCTags::TagMessageDetails& details) override
	{
		if (target.type == MessageTarget::TYPE_CHANNEL)
		{
			CUList unused;
			BuildChannelExempts(user, target.Get<Channel>(), SilenceEntry::SF_TAGMSG_CHANNEL, details.exemptions, unused);
			return MOD_RES_PASSTHRU;
		}

		if (target.type == MessageTarget::TYPE_USER && !CanReceiveMessage(user, target.Get<User>(), SilenceEntry::SF_TAGMSG_USER))
		{
			details.echo_original = true;
			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleSilence)
