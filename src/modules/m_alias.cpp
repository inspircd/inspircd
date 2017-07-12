/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2005, 2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2004-2007, 2009 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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
class Alias
{
 public:
	/** The text of the alias command */
	std::string AliasedCommand;

	/** Text to replace with */
	std::string ReplaceFormat;

	/** Nickname required to perform alias */
	std::string RequiredNick;

	/** Alias requires ulined server */
	bool ULineOnly;

	/** Requires oper? */
	bool OperOnly;

	/* is case sensitive params */
	bool CaseSensitive;

	/* whether or not it may be executed via fantasy (default OFF) */
	bool ChannelCommand;

	/* whether or not it may be executed via /command (default ON) */
	bool UserCommand;

	/** Format that must be matched for use */
	std::string format;
};

class ModuleAlias : public Module
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

 public:
	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* fantasy = ServerInstance->Config->ConfValue("fantasy");
		AllowBots = fantasy->getBool("allowbots", false);
		std::string fpre = fantasy->getString("prefix");
		fprefix = fpre.empty() ? "!" : fpre;

		Aliases.clear();
		ConfigTagList tags = ServerInstance->Config->ConfTags("alias");
		for(ConfigIter i = tags.first; i != tags.second; ++i)
		{
			ConfigTag* tag = i->second;
			Alias a;
			a.AliasedCommand = tag->getString("text");
			std::transform(a.AliasedCommand.begin(), a.AliasedCommand.end(), a.AliasedCommand.begin(), ::toupper);
			tag->readString("replace", a.ReplaceFormat, true);
			a.RequiredNick = tag->getString("requires");
			a.ULineOnly = tag->getBool("uline");
			a.ChannelCommand = tag->getBool("channelcommand", false);
			a.UserCommand = tag->getBool("usercommand", true);
			a.OperOnly = tag->getBool("operonly");
			a.format = tag->getString("format");
			a.CaseSensitive = tag->getBool("matchcase");
			Aliases.insert(std::make_pair(a.AliasedCommand, a));
		}
	}

	ModuleAlias()
		: botmode(this, "bot")
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides aliases of commands.", VF_VENDOR);
	}

	std::string GetVar(std::string varname, const std::string &original_line)
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

	ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, LocalUser *user, bool validated, const std::string &original_line) CXX11_OVERRIDE
	{
		/* If theyre not registered yet, we dont want
		 * to know.
		 */
		if (user->registered != REG_ALL)
			return MOD_RES_PASSTHRU;

		/* We dont have any commands looking like this? Stop processing. */
		std::pair<AliasMap::iterator, AliasMap::iterator> iters = Aliases.equal_range(command);
		if (iters.first == iters.second)
			return MOD_RES_PASSTHRU;

		/* The parameters for the command in their original form, with the command stripped off */
		std::string compare(original_line, command.length());
		while (*(compare.c_str()) == ' ')
			compare.erase(compare.begin());

		for (AliasMap::iterator i = iters.first; i != iters.second; ++i)
		{
			if (i->second.UserCommand)
			{
				if (DoAlias(user, NULL, &(i->second), compare, original_line))
				{
					return MOD_RES_DENY;
				}
			}
		}

		// If we made it here, no aliases actually matched.
		return MOD_RES_PASSTHRU;
	}

	void OnUserMessage(User *user, void *dest, int target_type, const std::string &text, char status, const CUList &exempt_list, MessageType msgtype) CXX11_OVERRIDE
	{
		if ((target_type != TYPE_CHANNEL) || (msgtype != MSG_PRIVMSG))
		{
			return;
		}

		// fcommands are only for local users. Spanningtree will send them back out as their original cmd.
		if (!IS_LOCAL(user))
		{
			return;
		}

		/* Stop here if the user is +B and allowbot is set to no. */
		if (!AllowBots && user->IsModeSet(botmode))
		{
			return;
		}

		Channel *c = (Channel *)dest;
		std::string scommand;

		// text is like "!moo cows bite me", we want "!moo" first
		irc::spacesepstream ss(text);
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

		std::pair<AliasMap::iterator, AliasMap::iterator> iters = Aliases.equal_range(scommand);
		if (iters.first == iters.second)
			return;

		/* The parameters for the command in their original form, with the command stripped off */
		std::string compare(text, scommand.length() + fprefix.size());
		while (*(compare.c_str()) == ' ')
			compare.erase(compare.begin());

		for (AliasMap::iterator i = iters.first; i != iters.second; ++i)
		{
			if (i->second.ChannelCommand)
			{
				// We use substr here to remove the fantasy prefix
				if (DoAlias(user, c, &(i->second), compare, text.substr(fprefix.size())))
					return;
			}
		}
	}


	int DoAlias(User *user, Channel *c, Alias *a, const std::string& compare, const std::string& safe)
	{
		/* Does it match the pattern? */
		if (!a->format.empty())
		{
			if (a->CaseSensitive)
			{
				if (!InspIRCd::Match(compare, a->format, rfc_case_sensitive_map))
					return 0;
			}
			else
			{
				if (!InspIRCd::Match(compare, a->format))
					return 0;
			}
		}

		if ((a->OperOnly) && (!user->IsOper()))
			return 0;

		if (!a->RequiredNick.empty())
		{
			User* u = ServerInstance->FindNick(a->RequiredNick);
			if (!u)
			{
				user->WriteNumeric(ERR_NOSUCHNICK, a->RequiredNick, "is currently unavailable. Please try again later.");
				return 1;
			}

			if ((a->ULineOnly) && (!u->server->IsULine()))
			{
				ServerInstance->SNO->WriteToSnoMask('a', "NOTICE -- Service "+a->RequiredNick+" required by alias "+a->AliasedCommand+" is not on a u-lined server, possibly underhanded antics detected!");
				user->WriteNumeric(ERR_NOSUCHNICK, a->RequiredNick, "is an imposter! Please inform an IRC operator as soon as possible.");
				return 1;
			}
		}

		/* Now, search and replace in a copy of the original_line, replacing $1 through $9 and $1- etc */

		std::string::size_type crlf = a->ReplaceFormat.find('\n');

		if (crlf == std::string::npos)
		{
			DoCommand(a->ReplaceFormat, user, c, safe, a);
			return 1;
		}
		else
		{
			irc::sepstream commands(a->ReplaceFormat, '\n');
			std::string scommand;
			while (commands.GetToken(scommand))
			{
				DoCommand(scommand, user, c, safe, a);
			}
			return 1;
		}
	}

	void DoCommand(const std::string& newline, User* user, Channel *chan, const std::string &original_line, Alias* a)
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
					int len = ((i + 2 < newline.length()) && (newline[i+2] == '-')) ? 3 : 2;
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
					result.append(user->host);
					i += 4;
				}
				else if (!newline.compare(i, 5, "$chan", 5))
				{
					if (chan)
						result.append(chan->name);
					i += 4;
				}
				else if (!newline.compare(i, 6, "$ident", 6))
				{
					result.append(user->ident);
					i += 5;
				}
				else if (!newline.compare(i, 6, "$vhost", 6))
				{
					result.append(user->dhost);
					i += 5;
				}
				else if (!newline.compare(i, 12, "$requirement", 12))
				{
					result.append(a->RequiredNick);
					i += 11;
				}
				else
					result.push_back(c);
			}
			else
				result.push_back(c);
		}

		irc::tokenstream ss(result);
		std::vector<std::string> pars;
		std::string command, token;

		ss.GetToken(command);
		while (ss.GetToken(token))
		{
			pars.push_back(token);
		}
		ServerInstance->Parser.CallHandler(command, pars, user);
	}

	void Prioritize() CXX11_OVERRIDE
	{
		// Prioritise after spanningtree so that channel aliases show the alias before the effects.
		Module* linkmod = ServerInstance->Modules->Find("m_spanningtree.so");
		ServerInstance->Modules->SetPriority(this, I_OnUserMessage, PRIORITY_AFTER, linkmod);
	}
};

MODULE_INIT(ModuleAlias)
