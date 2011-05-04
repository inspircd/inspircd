/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "command_parse.h"

/* $ModDesc: Provides aliases of commands. */

/** An alias definition
 */
class Alias
{
 public:
	/** The text of the alias command */
	irc::string AliasedCommand;

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

class CommandEcho : public Command
{
 public:
	CommandEcho(Module* Parent) : Command(Parent, "ECHO", 1, 1)
	{
		syntax = "<line>";
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		std::string line = parameters[0];
		if (line[0] != ':')
			user->WriteServ(line);
		else
			user->SendText(line);
		return CMD_SUCCESS;
	}
};

class AliasFormatSubst : public FormatSubstitute
{
 public:
	SubstMap info;
	const std::string &line;
	AliasFormatSubst(const std::string &Line) : line(Line) {}
	std::string lookup(const std::string& key)
	{
		if (isdigit(key[0]))
		{
			int index = atoi(key.c_str());
			irc::spacesepstream ss(line);
			bool everything_after = (key.find('-') != std::string::npos);
			bool good = true;
			std::string word;

			for (int j = 0; j < index && good; j++)
				good = ss.GetToken(word);

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
		SubstMap::iterator i = info.find(key);
		if (i != info.end())
			return i->second;
		return "";
	}
};

class ModuleAlias : public Module
{
	CommandEcho echo;
	char fprefix;
	/* We cant use a map, there may be multiple aliases with the same name.
	 * We can, however, use a fancy invention: the multimap. Maps a key to one or more values.
	 *		-- w00t
   */
	std::multimap<irc::string, Alias> Aliases;

	/* whether or not +B users are allowed to use fantasy commands */
	bool AllowBots;

	virtual void ReadAliases()
	{
		ConfigTag* tag = ServerInstance->Config->GetTag("fantasy");
		AllowBots = tag->getBool("allowbots");

		std::string fpre = tag->getString("prefix");
		fprefix = fpre.empty() ? '!' : fpre[0];

		Aliases.clear();
		ConfigTagList tags = ServerInstance->Config->GetTags("alias");
		for(ConfigIter i = tags.first; i != tags.second; ++i)
		{
			tag = i->second;
			Alias a;
			a.AliasedCommand = tag->getString("text").c_str();
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

 public:
	ModuleAlias() : echo(this) {}

	void init()
	{
		ReadAliases();
		ServerInstance->Modules->AddService(echo);
		ServerInstance->Modules->Attach(I_OnPreCommand, this);
		ServerInstance->Modules->Attach(I_OnUserMessage, this);
	}

	virtual ~ModuleAlias()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides aliases of commands.", VF_VENDOR);
	}

	virtual ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, LocalUser *user, bool validated, const std::string &original_line)
	{
		std::multimap<irc::string, Alias>::iterator i, upperbound;

		/* If theyre not registered yet, we dont want
		 * to know.
		 */
		if (user->registered != REG_ALL)
			return MOD_RES_PASSTHRU;

		/* We dont have any commands looking like this? Stop processing. */
		i = Aliases.find(command.c_str());
		if (i == Aliases.end())
			return MOD_RES_PASSTHRU;
		/* Avoid iterating on to different aliases if no patterns match. */
		upperbound = Aliases.upper_bound(command.c_str());

		irc::string c = command.c_str();
		/* The parameters for the command in their original form, with the command stripped off */
		std::string compare = original_line.substr(command.length());
		while (*(compare.c_str()) == ' ')
			compare.erase(compare.begin());

		while (i != upperbound)
		{
			if (i->second.UserCommand)
			{
				if (DoAlias(user, NULL, &(i->second), compare, original_line))
				{
					return MOD_RES_DENY;
				}
			}

			i++;
		}

		// If we made it here, no aliases actually matched.
		return MOD_RES_PASSTHRU;
	}

	virtual void OnUserMessage(User *u, void *dest, int target_type, const std::string &text, char status, const CUList &exempt_list)
	{
		if (target_type != TYPE_CHANNEL)
		{
			return;
		}

		LocalUser* user;
		// fcommands are only for local users. Spanningtree will send them back out as their original cmd.
		if (!u || !(user = IS_LOCAL(u)))
		{
			return;
		}

		/* Stop here if the user is +B and allowbot is set to no. */
		if (!AllowBots && user->IsModeSet('B'))
		{
			return;
		}

		Channel *c = (Channel *)dest;
		std::string scommand;

		// text is like "!moo cows bite me", we want "!moo" first
		irc::spacesepstream ss(text);
		ss.GetToken(scommand);

		if (scommand.empty())
		{
			return; // wtfbbq
		}

		// we don't want to touch non-fantasy stuff
		if (*scommand.c_str() != fprefix)
		{
			return;
		}

		// nor do we give a shit about the prefix
		scommand.erase(scommand.begin());

		irc::string fcommand = scommand;

		std::multimap<irc::string, Alias>::iterator i = Aliases.find(fcommand);

		if (i == Aliases.end())
			return;

		/* Avoid iterating on to other aliases if no patterns match */
		std::multimap<irc::string, Alias>::iterator upperbound = Aliases.upper_bound(fcommand);


		/* The parameters for the command in their original form, with the command stripped off */
		std::string compare = text.substr(scommand.length() + 1);
		while (*(compare.c_str()) == ' ')
			compare.erase(compare.begin());

		while (i != upperbound)
		{
			if (i->second.ChannelCommand)
			{
				// We use substr(1) here to remove the fantasy prefix
				if (DoAlias(user, c, &(i->second), compare, text.substr(1)))
					return;
			}

			i++;
		}
	}


	int DoAlias(LocalUser *user, Channel *c, Alias *a, const std::string& compare, const std::string& safe)
	{
		User *u = NULL;

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

		if ((a->OperOnly) && (!IS_OPER(user)))
			return 0;

		if (!a->RequiredNick.empty())
		{
			u = ServerInstance->FindNick(a->RequiredNick);
			if (!u)
			{
				user->WriteNumeric(401, ""+std::string(user->nick)+" "+a->RequiredNick+" :is currently unavailable. Please try again later.");
				return 1;
			}
		}
		if ((u != NULL) && (!a->RequiredNick.empty()) && (a->ULineOnly))
		{
			if (!ServerInstance->ULine(u->server))
			{
				ServerInstance->SNO->WriteToSnoMask('a', "NOTICE -- Service "+a->RequiredNick+" required by alias "+std::string(a->AliasedCommand.c_str())+" is not on a u-lined server, possibly underhanded antics detected!");
				user->WriteNumeric(401, ""+std::string(user->nick)+" "+a->RequiredNick+" :is an imposter! Please inform an IRC operator as soon as possible.");
				return 1;
			}
		}

		/* Now, search and replace in a copy of the original_line, replacing $1 through $9 and $1- etc */

		std::string::size_type crlf = a->ReplaceFormat.find('\n');

		if (crlf == std::string::npos)
		{
			DoCommand(a->ReplaceFormat, user, c, safe);
			return 1;
		}
		else
		{
			irc::sepstream commands(a->ReplaceFormat, '\n');
			std::string scommand;
			while (commands.GetToken(scommand))
			{
				DoCommand(scommand, user, c, safe);
			}
			return 1;
		}
	}

	void DoCommand(const std::string& newline, LocalUser* user, Channel *chan, const std::string &original_line)
	{
		AliasFormatSubst subst(original_line);
		user->PopulateInfoMap(subst.info);
		if (chan)
			subst.info["chan"] = chan->name;
		std::string result = subst.format(newline);

		irc::tokenstream ss(result);
		std::vector<std::string> pars;
		std::string command, token;

		ss.GetToken(command);
		while (ss.GetToken(token) && (pars.size() <= MAXPARAMETERS))
		{
			pars.push_back(token);
		}
		CmdResult res = ServerInstance->Parser->CallHandler(command, pars, user);
		FOREACH_MOD(I_OnPostCommand,OnPostCommand(command, pars, user, res,original_line));
	}

	void ReadConfig(ConfigReadStatus&)
	{
		ReadAliases();
 	}

	virtual void Prioritize()
	{
		// Prioritise after spanningtree so that channel aliases show the alias before the effects.
		Module* linkmod = ServerInstance->Modules->Find("m_spanningtree.so");
		ServerInstance->Modules->SetPriority(this, I_OnUserMessage, PRIORITY_AFTER, &linkmod);
	}
};

MODULE_INIT(ModuleAlias)
