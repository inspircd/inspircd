/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2015-2019, 2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2015, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Matt Smith <dz@inspircd.org>
 *   Copyright (C) 2008-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007-2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2004, 2006-2009 Craig Edwards <brain@inspircd.org>
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

/** An alias definition
 */
class Alias final
{
public:
	/** The text of the alias command */
	std::string AliasedCommand;

	/** Text to replace with */
	std::string ReplaceFormat;

	/** Nickname required to perform alias */
	std::string RequiredNick;

	/** Alias requires the required nick to be on a services server */
	bool ServiceOnly;

	/** Requires oper? */
	bool OperOnly;

	/* whether or not it may be executed via fantasy (default OFF) */
	bool ChannelCommand;

	/* whether or not it may be executed via /command (default ON) */
	bool UserCommand;

	/** Format that must be matched for use */
	std::string format;

	/** Strip color codes before match? */
	bool StripColor;
};

class ModuleAlias final
	: public Module
{
	std::string fprefix;

	/* We cant use a map, there may be multiple aliases with the same name.
	 * We can, however, use a fancy invention: the multimap. Maps a key to one or more values.
	 *		-- w00t
	 */
	typedef insp::flat_multimap<std::string, Alias, irc::insensitive_swo> AliasMap;

	AliasMap Aliases;

	/* whether or not +B users are allowed to use fantasy commands */
	bool AllowBots;
	UserModeReference botmode;

	// Whether we are actively executing an alias.
	bool active = false;

public:
	void ReadConfig(ConfigStatus& status) override
	{
		AliasMap newAliases;
		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("alias"))
		{
			Alias a;
			a.AliasedCommand = tag->getString("text");
			if (a.AliasedCommand.empty())
				throw ModuleException(this, "<alias:text> is empty! at " + tag->source.str());

			tag->readString("replace", a.ReplaceFormat, true);
			if (a.ReplaceFormat.empty())
				throw ModuleException(this, "<alias:replace> is empty! at " + tag->source.str());

			a.RequiredNick = tag->getString("requires");
			a.ServiceOnly = tag->getBool("service", tag->getBool("uline"));
			a.ChannelCommand = tag->getBool("channelcommand", false);
			a.UserCommand = tag->getBool("usercommand", true);
			a.OperOnly = tag->getBool("operonly");
			a.format = tag->getString("format");
			a.StripColor = tag->getBool("stripcolor");

			std::transform(a.AliasedCommand.begin(), a.AliasedCommand.end(), a.AliasedCommand.begin(), ::toupper);
			newAliases.emplace(a.AliasedCommand, a);
		}

		const auto& fantasy = ServerInstance->Config->ConfValue("fantasy");
		AllowBots = fantasy->getBool("allowbots", false);
		fprefix = fantasy->getString("prefix", "!", 1, ServerInstance->Config->Limits.MaxLine);
		Aliases.swap(newAliases);
	}

	ModuleAlias()
		: Module(VF_VENDOR, "Allows the server administrator to define custom channel commands (e.g. !kick) and server commands (e.g. /OPERSERV).")
		, botmode(this, "bot")
	{
	}

	static std::string GetVar(std::string varname, const std::string& original_line)
	{
		irc::spacesepstream ss(original_line);
		varname.erase(varname.begin());
		int index = *(varname.begin()) - 48;
		varname.erase(varname.begin());
		bool everything_after = (varname == "-");
		std::string word;

		for (int j = 0; j < index; j++)
			ss.GetToken(word);

		if (everything_after)
		{
			std::string more;
			while (ss.GetToken(more))
			{
				word.append(" ");
				word.append(more);
			}
		}

		return word;
	}

	static std::string CreateRFCMessage(const std::string& command, CommandBase::Params& parameters)
	{
		std::string message(command);
		for (CommandBase::Params::const_iterator iter = parameters.begin(); iter != parameters.end();)
		{
			const std::string& parameter = *iter++;
			message.push_back(' ');
			if (iter == parameters.end() && (parameter.empty() || parameter.find(' ') != std::string::npos))
				message.push_back(':');
			message.append(parameter);
		}
		return message;
	}

	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) override
	{
		// If they're not fully connected yet, we dont want to know.
		if (!user->IsFullyConnected())
			return MOD_RES_PASSTHRU;

		/* We dont have any commands looking like this? Stop processing. */
		auto aliases = insp::equal_range(Aliases, command);
		if (aliases.empty())
			return MOD_RES_PASSTHRU;

		/* The parameters for the command in their original form, with the command stripped off */
		std::string original_line = CreateRFCMessage(command, parameters);
		std::string compare(original_line, command.length());
		while (*(compare.c_str()) == ' ')
			compare.erase(compare.begin());

		for (const auto& [_, alias] : aliases)
		{
			if (alias.UserCommand)
			{
				if (DoAlias(user, nullptr, alias, compare, original_line))
				{
					return MOD_RES_DENY;
				}
			}
		}

		// If we made it here, no aliases actually matched.
		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreMessage(User* user, const MessageTarget& target, MessageDetails& details) override
	{
		// Don't echo anything which is caused by an alias.
		if (active)
			details.echo = false;

		return MOD_RES_PASSTHRU;
	}

	void OnUserPostMessage(User* user, const MessageTarget& target, const MessageDetails& details) override
	{
		// Don't allow aliases to trigger other aliases.
		if (active)
			return;

		if ((target.type != MessageTarget::TYPE_CHANNEL) || (details.type != MessageType::PRIVMSG))
		{
			return;
		}

		// fcommands are only for local users. Spanningtree will send them back out as their original cmd.
		LocalUser* luser = IS_LOCAL(user);
		if (!luser)
		{
			return;
		}

		/* Stop here if the user is +B and allowbot is set to no. */
		if (!AllowBots && user->IsModeSet(botmode))
		{
			return;
		}

		Channel* c = target.Get<Channel>();
		std::string scommand;

		// text is like "!moo cows bite me", we want "!moo" first
		irc::spacesepstream ss(details.text);
		ss.GetToken(scommand);

		if (scommand.size() <= fprefix.size())
		{
			return; // wtfbbq
		}

		// we don't want to touch non-fantasy stuff
		if (scommand.compare(0, fprefix.size(), fprefix) != 0)
		{
			return;
		}

		// nor do we give a shit about the prefix
		scommand.erase(0, fprefix.size());

		auto aliases = insp::equal_range(Aliases, scommand);
		if (aliases.empty())
			return;

		/* The parameters for the command in their original form, with the command stripped off */
		std::string compare(details.text, scommand.length() + fprefix.size());
		while (*(compare.c_str()) == ' ')
			compare.erase(compare.begin());

		for (const auto& [_, alias] : aliases)
		{
			if (alias.ChannelCommand)
			{
				// We use substr here to remove the fantasy prefix
				if (DoAlias(luser, c, alias, compare, details.text.substr(fprefix.size())))
					return;
			}
		}
	}

	int DoAlias(LocalUser* user, Channel* c, const Alias& a, const std::string& compare, const std::string& safe)
	{
		std::string stripped(compare);
		if (a.StripColor)
			InspIRCd::StripColor(stripped);

		/* Does it match the pattern? */
		if (!a.format.empty())
		{
			if (!InspIRCd::Match(stripped, a.format))
				return 0;
		}

		if ((a.OperOnly) && (!user->IsOper()))
			return 0;

		if (!a.RequiredNick.empty())
		{
			int numeric = a.ServiceOnly ? ERR_NOSUCHSERVICE : ERR_NOSUCHNICK;
			auto* u = ServerInstance->Users.FindNick(a.RequiredNick);
			if (!u)
			{
				user->WriteNumeric(numeric, a.RequiredNick, "is currently unavailable. Please try again later.");
				return 1;
			}

			if ((a.ServiceOnly) && (!u->server->IsService()))
			{
				ServerInstance->SNO.WriteToSnoMask('a', "NOTICE -- Service "+a.RequiredNick+" required by alias "+a.AliasedCommand+" is not on a services server, possibly underhanded antics detected!");
				user->WriteNumeric(numeric, a.RequiredNick, "is not a network service! Please inform a server operator as soon as possible.");
				return 1;
			}
		}

		/* Now, search and replace in a copy of the original_line, replacing $1 through $9 and $1- etc */

		std::string::size_type crlf = a.ReplaceFormat.find('\n');

		if (crlf == std::string::npos)
		{
			DoCommand(a.ReplaceFormat, user, c, safe, a);
			return 1;
		}
		else
		{
			irc::sepstream commands(a.ReplaceFormat, '\n');
			std::string scommand;
			while (commands.GetToken(scommand))
			{
				DoCommand(scommand, user, c, safe, a);
			}
			return 1;
		}
	}

	void DoCommand(const std::string& newline, LocalUser* user, Channel* chan, const std::string& original_line, const Alias& a)
	{
		std::string result;
		result.reserve(newline.length());
		for (unsigned int i = 0; i < newline.length(); i++)
		{
			char c = newline[i];
			if ((c == '$') && (i + 1 < newline.length()))
			{
				if (isdigit(newline[i+1]))
				{
					size_t len = ((i + 2 < newline.length()) && (newline[i+2] == '-')) ? 3 : 2;
					std::string var = newline.substr(i, len);
					result.append(GetVar(var, original_line));
					i += len - 1;
				}
				else if (!newline.compare(i, 5, "$nick", 5))
				{
					result.append(user->nick);
					i += 4;
				}
				else if (!newline.compare(i, 5, "$host", 5))
				{
					result.append(user->GetRealHost());
					i += 4;
				}
				else if (!newline.compare(i, 5, "$chan", 5))
				{
					if (chan)
						result.append(chan->name);
					i += 4;
				}
				else if (!newline.compare(i, 5, "$user", 5))
				{
					result.append(user->GetRealUser());
					i += 4;
				}
				else if (!newline.compare(i, 6, "$vhost", 6))
				{
					result.append(user->GetDisplayedHost());
					i += 5;
				}
				else if (!newline.compare(i, 12, "$requirement", 12))
				{
					result.append(a.RequiredNick);
					i += 11;
				}
				else if (!newline.compare(i, 6, "$address", 8))
				{
					result.append(user->GetAddress());
					i += 7;
				}
				else if (!newline.compare(i, 6, "$ident", 6))
				{
					// TODO: remove this in the next major release.
					result.append(user->GetRealHost());
					i += 5;
				}

				else
					result.push_back(c);
			}
			else
				result.push_back(c);
		}

		irc::tokenstream ss(result);
		CommandBase::Params pars;
		std::string command;
		std::string token;

		ss.GetMiddle(command);
		while (ss.GetTrailing(token))
		{
			pars.push_back(token);
		}

		active = true;
		ServerInstance->Parser.ProcessCommand(user, command, pars);
		active = false;
	}

	void Prioritize() override
	{
		// Prioritise after spanningtree so that channel aliases show the alias before the effects.
		Module* linkmod = ServerInstance->Modules.Find("spanningtree");
		ServerInstance->Modules.SetPriority(this, I_OnUserPostMessage, PRIORITY_AFTER, linkmod);
	}
};

MODULE_INIT(ModuleAlias)
