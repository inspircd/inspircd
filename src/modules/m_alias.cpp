/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

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

class ModuleAlias : public Module
{
 private:

	char fprefix;

	/* We cant use a map, there may be multiple aliases with the same name.
	 * We can, however, use a fancy invention: the multimap. Maps a key to one or more values.
	 *		-- w00t
   */
	std::multimap<std::string, Alias> Aliases;

	/* whether or not +B users are allowed to use fantasy commands */
	bool AllowBots;

	virtual void ReadAliases()
	{
		ConfigReader MyConf;

		AllowBots = MyConf.ReadFlag("fantasy", "allowbots", "no", 0);

		std::string fpre = MyConf.ReadValue("fantasy","prefix",0);
		fprefix = fpre.empty() ? '!' : fpre[0];

		Aliases.clear();
		for (int i = 0; i < MyConf.Enumerate("alias"); i++)
		{
			Alias a;
			std::string txt;
			txt = MyConf.ReadValue("alias", "text", i);
			a.AliasedCommand = txt.c_str();
			a.ReplaceFormat = MyConf.ReadValue("alias", "replace", i, true);
			a.RequiredNick = MyConf.ReadValue("alias", "requires", i);
			a.ULineOnly = MyConf.ReadFlag("alias", "uline", i);
			a.ChannelCommand = MyConf.ReadFlag("alias", "channelcommand", "no", i);
			a.UserCommand = MyConf.ReadFlag("alias", "usercommand", "yes", i);
			a.OperOnly = MyConf.ReadFlag("alias", "operonly", i);
			a.format = MyConf.ReadValue("alias", "format", i);
			a.CaseSensitive = MyConf.ReadFlag("alias", "matchcase", i);
			Aliases.insert(std::make_pair(txt, a));
		}
	}

 public:

	ModuleAlias()
			{
		ReadAliases();
		ServerInstance->Modules->Attach(I_OnPreCommand, this);
		ServerInstance->Modules->Attach(I_OnRehash, this);
		ServerInstance->Modules->Attach(I_OnUserMessage, this);

	}

	virtual ~ModuleAlias()
	{
	}

	virtual Version GetVersion()
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

	virtual ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, User *user, bool validated, const std::string &original_line)
	{
		std::multimap<std::string, Alias>::iterator i, upperbound;

		/* If theyre not registered yet, we dont want
		 * to know.
		 */
		if (user->registered != REG_ALL)
			return MOD_RES_PASSTHRU;

		/* We dont have any commands looking like this? Stop processing. */
		i = Aliases.find(command);
		if (i == Aliases.end())
			return MOD_RES_PASSTHRU;
		/* Avoid iterating on to different aliases if no patterns match. */
		upperbound = Aliases.upper_bound(command);

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

	virtual void OnUserMessage(User *user, void *dest, int target_type, const std::string &text, char status, const CUList &exempt_list)
	{
		if (target_type != TYPE_CHANNEL)
		{
			return;
		}

		// fcommands are only for local users. Spanningtree will send them back out as their original cmd.
		if (!user || !IS_LOCAL(user))
		{
			return;
		}

		/* Stop here if the user is +B and allowbot is set to no. */
		if (!AllowBots && user->IsModeSet('B'))
		{
			return;
		}

		Channel *c = (Channel *)dest;
		std::string fcommand;

		// text is like "!moo cows bite me", we want "!moo" first
		irc::spacesepstream ss(text);
		ss.GetToken(fcommand);

		if (fcommand.empty())
		{
			return; // wtfbbq
		}

		// we don't want to touch non-fantasy stuff
		if (*fcommand.c_str() != fprefix)
		{
			return;
		}

		// nor do we give a shit about the prefix
		fcommand.erase(fcommand.begin());
		std::transform(fcommand.begin(), fcommand.end(), fcommand.begin(), ::toupper);

		std::multimap<std::string, Alias>::iterator i = Aliases.find(fcommand);

		if (i == Aliases.end())
			return;

		/* Avoid iterating on to other aliases if no patterns match */
		std::multimap<std::string, Alias>::iterator upperbound = Aliases.upper_bound(fcommand);


		/* The parameters for the command in their original form, with the command stripped off */
		std::string compare = text.substr(fcommand.length() + 1);
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


	int DoAlias(User *user, Channel *c, Alias *a, const std::string compare, const std::string safe)
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

	void DoCommand(std::string newline, User* user, Channel *c, const std::string &original_line)
	{
		std::vector<std::string> pars;

		for (int v = 1; v < 10; v++)
		{
			std::string var = "$";
			var.append(ConvToStr(v));
			var.append("-");
			std::string::size_type x = newline.find(var);

			while (x != std::string::npos)
			{
				newline.erase(x, var.length());
				newline.insert(x, GetVar(var, original_line));
				x = newline.find(var);
			}

			var = "$";
			var.append(ConvToStr(v));
			x = newline.find(var);

			while (x != std::string::npos)
			{
				newline.erase(x, var.length());
				newline.insert(x, GetVar(var, original_line));
				x = newline.find(var);
			}
		}

		/* Special variables */
		SearchAndReplace(newline, std::string("$nick"), user->nick);
		SearchAndReplace(newline, std::string("$ident"), user->ident);
		SearchAndReplace(newline, std::string("$host"), user->host);
		SearchAndReplace(newline, std::string("$vhost"), user->dhost);

		if (c)
		{
			/* Channel specific variables */
			SearchAndReplace(newline, std::string("$chan"), c->name);
		}
		else
		{
			/* We don't want these in a user alias */
			SearchAndReplace(newline, std::string("$chan"), std::string(""));
		}

		irc::tokenstream ss(newline);
		pars.clear();
		std::string command, token;

		ss.GetToken(command);
		while (ss.GetToken(token) && (pars.size() <= MAXPARAMETERS))
		{
			pars.push_back(token);
		}
		ServerInstance->Parser->CallHandler(command, pars, user);
	}

	virtual void OnRehash(User* user)
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
