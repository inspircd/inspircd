/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2026 Sadie Powell <sadie@witchery.services>
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
#include "extension.h"
#include "modules/ircv3_replies.h"
#include "modules/server.h"
#include "modules/whois.h"
#include "numerichelper.h"

struct SWhois final
{
	// Whether this is a compatibility swhois message.
	bool compat = false;

	// Whether this swhois comes from an oper config.
	bool oper = false;

	// Where to prioritize this entry in the output.
	time_t priority = ServerInstance->Time();

	// The swhois message.
	std::string message;

	// The tag for referencing this message in S2S.
	std::string tag;

	bool operator <(const SWhois& other) const
	{
		return std::tie(priority, tag, message, oper, compat)
			< std::tie(other.priority, other.tag, other.message, other.oper, other.compat);
	}

	std::string GetFlags() const
	{
		std::string flags;
		if (this->compat)
			flags.push_back('c');
		if (this->oper)
			flags.push_back('o');
		if (flags.empty())
			flags.push_back('*');
		return flags;
	}

	std::string SerializeAdd() const
	{
		return FMT::format("+ @{} {} {} :{}", this->tag, this->GetFlags(),
			this->priority, this->message);
	}

	std::string SerializeDel() const
	{
		if (this->tag.empty())
			return FMT::format("- :{}", this->message);
		else
			return FMT::format("- @{}", this->tag);
	}
};

using SWhoisList = std::vector<SWhois>;

using SWhoisExtItem = SimpleExtItem<SWhoisList>;

namespace
{
	static SWhois& AddSWhois(SWhoisExtItem& swhoisext, User* user, const std::string& msg)
	{
		auto* swhoislist = swhoisext.Get(user);
		if (!swhoislist)
		{
			swhoislist = new SWhoisList();
			swhoisext.Set(user, swhoislist);
		}

		// If the message is empty we use a space to avoid client formatting issues.
		SWhois swhois;
		swhois.message = msg.empty() ? " " : msg;

		// Insert sorted so we get the right order on iteration.
		auto pos = std::upper_bound(swhoislist->begin(), swhoislist->end(), swhois);
		return *swhoislist->insert(pos, swhois);
	}

	static bool DelSWhois(SWhoisExtItem& swhoisext, User* user, std::function<bool(SWhois&)> predicate, bool sync)
	{
		auto* swhoislist = swhoisext.Get(user);
		if (!swhoislist)
			return false; // Nothing to delete.

		auto it = std::remove_if(swhoislist->begin(), swhoislist->end(), predicate);
		if (it == swhoislist->end())
			return false; // Nothing deleted.

		if (sync)
		{
			for (auto sit = it; sit != swhoislist->end(); ++sit)
			{
				if (sit->compat)
					ServerInstance->PI->SendMetadata(user, "swhois", "");
				else
					ServerInstance->PI->SendMetadata(user, "specialwhois", sit->SerializeDel());
			}
		}

		swhoislist->erase(it, swhoislist->end());
		if (swhoislist->empty())
			swhoisext.Unset(user);
		return true;
	}
}

class CommandSWhois final
	: public SplitCommand
{
public:
	SWhoisExtItem swhoisext;

private:
	IRCv3::Replies::Note failrpl;
	IRCv3::Replies::Note noterpl;
	IRCv3::Replies::CapReference stdrplcap;

	CmdResult DoAdd(LocalUser* source, User* target, const Params& parameters)
	{
		if (parameters.size() < 3)
		{
			TellNotEnoughParameters(source, parameters);
			return CmdResult::FAILURE;
		}

		auto& swhois = AddSWhois(swhoisext, target, parameters.back());
		if (parameters.size() > 3 && parameters[3].find_first_not_of("0123456789+-") == std::string::npos)
			swhois.priority = ConvToNum<time_t>(parameters[2]);
		ServerInstance->PI->SendMetadata(target, "specialwhois", swhois.SerializeAdd());

		noterpl.SendIfCap(source, stdrplcap, this, "ENTRY_ADDED", target->nick, FMT::format("Added special whois for {}: {}",
			target->nick, swhois.message));
		return CmdResult::SUCCESS;
	}

	CmdResult DoClear(LocalUser* source, User* target, const Params& parameters)
	{
		if (DelSWhois(swhoisext, source, [](const SWhois& swhois) { return true; }, true))
		{
			noterpl.SendIfCap(source, stdrplcap, this, "LIST_CLEARED", target->nick, FMT::format("Special whois list for {} has been cleared.",
				target->nick));
		}
		else
		{
			failrpl.SendIfCap(source, stdrplcap, this, "LIST_EMPTY", target->nick, FMT::format("Special whois list for {} is already empty!",
				target->nick));
		}
		return CmdResult::SUCCESS;
	}

	CmdResult DoDel(LocalUser* source, User* target, const Params& parameters)
	{
		if (parameters.size() < 3)
		{
			TellNotEnoughParameters(source, parameters);
			return CmdResult::FAILURE;
		}

		auto deleted = false;
		const auto& msg = parameters[2];
		if (msg.find_first_not_of("0123456789") == std::string::npos)
		{
			size_t currentidx = 0;;
			const auto idx = ConvToNum<size_t>(msg);
			deleted = DelSWhois(swhoisext, source, [&currentidx, idx](const SWhois& swhois) {
				return ++currentidx == idx;
			}, true);
		}
		if (!deleted)
		{
			deleted = DelSWhois(swhoisext, source, [&msg](const SWhois& swhois) {
				return msg == swhois.message;
			}, true);
		}

		if (deleted)
		{
			noterpl.SendIfCap(source, stdrplcap, this, "ENTRY_DELETED", target->nick,
				"The special whois message you specified has been deleted.");
		}
		else
		{
			failrpl.SendIfCap(source, stdrplcap, this, "LIST_EMPTY", target->nick,
				"The special whois message you specified does not exist!");
		}
		return CmdResult::SUCCESS;
	}

	CmdResult DoList(LocalUser* source, User* target, const Params& parameters)
	{
		auto* swhoislist = swhoisext.Get(target);
		if (!swhoislist || swhoislist->empty())
		{
			failrpl.SendIfCap(source, stdrplcap, this, "LIST_EMPTY", target->nick, FMT::format("Special whois list for {} is empty!",
				target->nick));
			return CmdResult::SUCCESS;
		}

		size_t index = 0;
		for (const auto& swhois : *swhoislist)
		{
			noterpl.SendIfCap(source, stdrplcap, this, "LIST_ENTRY", target->nick, FMT::format("#{}: {} (priority: {}, flags: {})",
				++index, swhois.message, swhois.priority, swhois.GetFlags()));
		}

		return CmdResult::SUCCESS;
	}

public:
	CommandSWhois(Module* mod)
		: SplitCommand(mod, "SWHOIS", 2, 4)
		, swhoisext(mod, "swhois", ExtensionType::USER)
		, failrpl(mod)
		, noterpl(mod)
		, stdrplcap(mod)
	{
		access_needed = CmdAccess::OPERATOR;
		allow_empty_last_param = true;
		syntax = {
			"ADD <nick> [<priority>] :<message>",
			"CLEAR <nick>",
			"DEL <nick> :<index|message>",
			"LIST <nick>",
		};
		translation = { TR_TEXT, TR_NICK, TR_TEXT, TR_TEXT };
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		auto* target = ServerInstance->Users.Find(parameters[1]);
		if (!target)
		{
			user->WriteNumeric(Numerics::NoSuchNick(parameters[1]));
			return CmdResult::FAILURE;
		}

		const auto& subcmd = parameters[0];
		if (irc::equals(subcmd, "ADD"))
			return DoAdd(user, target, parameters);
		else if (irc::equals(subcmd, "CLEAR"))
			return DoClear(user, target, parameters);
		else if (irc::equals(subcmd, "DEL"))
			return DoDel(user, target, parameters);
		else if (irc::equals(subcmd, "LIST"))
			return DoList(user, target, parameters);
		else
		{
			failrpl.SendIfCap(user, stdrplcap, this, "UNKNOWN_COMMAND", subcmd, FMT::format("Invalid {} subcommand: {}",
				this->name, subcmd));

			if (ServerInstance->Config->SyntaxHints)
			{
				for (const auto& syntaxline : this->syntax)
					user->WriteNumeric(RPL_SYNTAX, name, syntaxline);
			}
			return CmdResult::FAILURE;
		}
		return CmdResult::SUCCESS;
	}
};

class ModuleSWhois final
	: public Module
	, public ServerProtocol::SyncEventListener
	, public Whois::LineEventListener
{
private:
	CommandSWhois cmdswhois;
	UserModeReference hideopermode;

	void DecodeSWhoisAdd(User* user, irc::tokenstream& stream)
	{
		std::string tag, flags, priority, message;
		if (!stream.GetMiddle(tag) || !stream.GetMiddle(flags) || !stream.GetMiddle(priority) || !stream.GetTrailing(message))
			return; // Malformed.

		// If a tag was specified we might be replacing an existing entry.
		if (tag.length() > 1 && tag[0] == '@')
		{
			tag.erase(0, 1);
			DelSWhois(cmdswhois.swhoisext, user, [&tag](const SWhois& swhois) {
				return swhois.tag == tag;
			}, false);
		}

		auto& swhois = AddSWhois(cmdswhois.swhoisext, user, message);
		swhois.tag = tag;
		swhois.oper = flags.find('o') == std::string::npos;
		swhois.priority = ConvToNum<time_t>(priority);
	}

	void DecodeSWhoisDel(User* user, irc::tokenstream& stream)
	{
		std::string message;
		if (!stream.GetTrailing(message))
			return; // Malformed.

		auto deleted = false;
		if (message.length() > 1 && message[0] == '@')
		{
			auto tag = message.substr(1);
			deleted = DelSWhois(cmdswhois.swhoisext, user, [&tag](const SWhois& swhois) {
				return swhois.tag == tag;
			}, false);
		}
		if (!deleted)
		{
			DelSWhois(cmdswhois.swhoisext, user, [&message](const SWhois& swhois) {
				return swhois.message == message;
			}, false);
		}
	}

	void DecodeSWhoisLegacy(User* user, const std::string& message)
	{
		// Delete the previous compatibility swhois message and optionally replace it.
		DelSWhois(cmdswhois.swhoisext, user, [](const SWhois& swhois) { return swhois.compat; }, false);
		if (!message.empty())
			AddSWhois(cmdswhois.swhoisext, user, message);
	}

public:
	ModuleSWhois()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds the /SWHOIS command which adds custom messages to a user's WHOIS response.")
		, ServerProtocol::SyncEventListener(this)
		, Whois::LineEventListener(this)
		, cmdswhois(this)
		, hideopermode(this, "hideoper")
	{
	}

	void OnDecodeMetadata(Extensible* target, const std::string& extname, const std::string& extvalue) override
	{
		if (!target || target->extype != ExtensionType::USER)
			return; // Not for us

		auto* user = static_cast<User*>(target);
		if (irc::equals(extname, "swhois"))
			DecodeSWhoisLegacy(user, extvalue);
		else if (irc::equals(extname, "specialwhois"))
		{
			irc::tokenstream msgstream(extvalue);

			std::string operation;
			if (!msgstream.GetMiddle(operation))
				return; // Malformed.

			if (irc::equals(operation, "+"))
				DecodeSWhoisAdd(user, msgstream);

			else if (irc::equals(operation, "-"))
				DecodeSWhoisDel(user, msgstream);
		}
	}

	void OnPostOperLogin(User* user, bool automatic) override
	{
		if (!IS_LOCAL(user))
			return;

		std::string swhoisstr;
		if (!user->oper->GetConfig()->readString("swhois", swhoisstr, true) || swhoisstr.empty())
			return;

		irc::sepstream msgstream(swhoisstr, '\n', true);
		for (std::string msg; msgstream.GetToken(msg); )
		{
			auto& swhois = AddSWhois(cmdswhois.swhoisext, user, msg);
			swhois.oper = true;
			ServerInstance->PI->SendMetadata(user, "specialwhois", swhois.SerializeAdd());
		}
	}

	void OnPostOperLogout(User* user, const std::shared_ptr<OperAccount>& oper) override
	{
		if (!IS_LOCAL(user))
			return;

		// Remove any swhois entries added by the oper block.
		DelSWhois(cmdswhois.swhoisext, user, [](const SWhois& swhois) { return swhois.oper; }, true);
	}

	void OnSyncUser(User* user, Server& server) override
	{
		auto* swhoislist = cmdswhois.swhoisext.Get(user);
		if (!swhoislist)
			return;

		for (const auto& swhois : *swhoislist)
		{
			if (swhois.compat)
				server.SendMetadata(user, "swhois", swhois.message);
			else
				server.SendMetadata(user, "specialwhois", swhois.SerializeAdd());
		}
	}

	ModResult OnWhoisLine(Whois::Context& whois, Numeric::Numeric& numeric) override
	{
		// We use this and not OnWhois because this triggers for remote users too.
		if (numeric.GetNumeric() != RPL_WHOISSERVER)
			return MOD_RES_PASSTHRU;

		auto* swhoislist = cmdswhois.swhoisext.Get(whois.GetTarget());
		if (!swhoislist)
			return MOD_RES_PASSTHRU;

		for (const auto& swhois : *swhoislist)
		{
			if (swhois.oper && whois.GetTarget()->IsModeSet(hideopermode))
				continue; // Avoid exposing hidden opers.

			whois.SendLine(RPL_WHOISSPECIAL, swhois.message);
		}
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleSWhois)
