/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Provides fake clients that respond to messages. */

/** Command definition
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

	/** RequiredNick must be on a ulined server */
	bool ULineOnly;

	/** Format that must be matched for use */
	std::string format;
};

typedef std::multimap<irc::string, Alias>::iterator AliasIter;

class ServerBot : public FakeUser
{
 public:
	ServerBot(const std::string& uid) : FakeUser(uid, ServerInstance->Config->ServerName) {}
	virtual const std::string& GetFullHost()
	{
		return this->User::GetFullHost();
	}
};

class BotData
{
 public:
	std::multimap<irc::string, Alias> Aliases;
	ServerBot* const bot;
	BotData(ServerBot* Bot) : bot(Bot) {}

	void HandleMessage(User* user, const std::string& text)
	{
		irc::spacesepstream ss(text);
		std::string command, params;
		ss.GetToken(command);
		params = ss.GetRemaining();

		std::pair<AliasIter, AliasIter> range = Aliases.equal_range(assign(command));

		for(AliasIter i = range.first; i != range.second; i++)
		{
			DoAlias(user, &i->second, params, text);
		}

		// also support no-command aliases (presumably they have format checks)
		range = Aliases.equal_range("");
		for(AliasIter i = range.first; i != range.second; i++)
		{
			DoAlias(user, &i->second, text, text);
		}
	}

	void DoAlias(User *user, Alias *a, const std::string& params, const std::string& text)
	{
		/* Does it match the pattern? */
		if (!a->format.empty())
		{
			if (!InspIRCd::Match(params, a->format))
				return;
		}

		if (!a->RequiredNick.empty())
		{
			User* u = ServerInstance->FindNick(a->RequiredNick);
			if (!u)
			{
				user->WriteFrom(bot, "NOTICE %s :%s is currently unavailable. Please try again later.",
					user->nick.c_str(), a->RequiredNick.c_str());
				return;
			}
			if (a->ULineOnly && !ServerInstance->ULine(u->server))
			{
				ServerInstance->SNO->WriteToSnoMask('a', "NOTICE -- Service "+a->RequiredNick+" required by alias "+std::string(a->AliasedCommand.c_str())+" is not on a u-lined server, possibly underhanded antics detected!");
				user->WriteFrom(bot, "NOTICE %s :%s is an imposter! Please inform an IRC operator as soon as possible.",
					user->nick.c_str(), a->RequiredNick.c_str());
				return;
			}
		}

		/* Now, search and replace in a copy of the original_line, replacing $1 through $9 and $1- etc */

		irc::sepstream commands(a->ReplaceFormat, '\n');
		std::string scommand;
		while (commands.GetToken(scommand))
		{
			DoCommand(scommand, user, text);
		}
	}

	void DoCommand(const std::string& format, User* user, const std::string &text)
	{
		std::string result;
		result.reserve(MAXBUF);
		for (unsigned int i = 0; i < format.length(); i++)
		{
			char c = format[i];
			if (c == '$')
			{
				if (isdigit(format[i+1]))
				{
					int len = (format[i+2] == '-') ? 3 : 2;
					std::string var = format.substr(i, len);
					result.append(GetVar(var, text));
					i += len - 1;
				}
				else if (format.substr(i, 4) == "$bot")
				{
					result.append(bot->nick);
					i += 3;
				}
				else if (format.substr(i, 5) == "$nick")
				{
					result.append(user->nick);
					i += 4;
				}
				else if (format.substr(i, 5) == "$host")
				{
					result.append(user->host);
					i += 4;
				}
				else if (format.substr(i, 6) == "$ident")
				{
					result.append(user->ident);
					i += 5;
				}
				else if (format.substr(i, 6) == "$vhost")
				{
					result.append(user->dhost);
					i += 5;
				}
				else if (format.substr(i, 8) == "$fullbot")
				{
					result.append(bot->GetFullHost());
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
		while (ss.GetToken(token) && (pars.size() <= MAXPARAMETERS))
		{
			pars.push_back(token);
		}
		ServerInstance->Parser->CallHandler(command, pars, user);
	}

	std::string GetVar(std::string varname, const std::string &original_line)
	{
		irc::spacesepstream ss(original_line);
		int index = varname[1] - '0';
		bool everything_after = (varname.length() == 3);
		std::string word;

		for (int j = 0; j < index; j++)
			ss.GetToken(word);

		if (everything_after)
			return ss.GetRemaining();

		ss.GetToken(word);
		return word;
	}
};

class ModuleServerBots : public Module
{
	std::map<std::string, BotData*> bots;
	SimpleExtItem<BotData> dataExt;
	bool recursing;
	int botID;

 public:
	ModuleServerBots() : dataExt("serverbot", this) {}

	void init()
	{
		recursing = false;
		botID = 0;
		ServerInstance->Modules->Attach(I_OnUserMessage, this);
	}

	Version GetVersion()
	{
		return Version("Provides fake clients for handling IRCd commands.", VF_VENDOR);
	}

	void OnUserMessage(User *user, void *dest, int target_type, const std::string &text, char status, const CUList &exempt_list)
	{
		if (target_type != TYPE_USER)
			return;
		User* b = (User*)dest;
		BotData* bot = dataExt.get(b);
		if (!bot)
			return;

		if (recursing)
		{
			user->WriteFrom(bot->bot, "NOTICE %s :Your command caused a recursive bot message which was not processed.", user->nick.c_str());
			return;
		}

		recursing = true;
		bot->HandleMessage(user, text);
		recursing = false;
	}

	void ReadConfig(ConfigReadStatus&)
	{
		std::map<std::string, BotData*> oldbots;
		oldbots.swap(bots);

		ConfigTagList tags = ServerInstance->Config->ConfTags("bot");
		for(ConfigIter i = tags.first; i != tags.second; i++)
		{
			ConfigTag* tag = i->second;
			// UID is of the form "12!BOT"
			std::string nick = tag->getString("nick");
			if (nick.empty())
				continue;
			std::map<std::string, BotData*>::iterator found = oldbots.find(nick);
			ServerBot* bot;
			if (found != oldbots.end())
			{
				bots.insert(*found);
				bot = found->second->bot;
				found->second->Aliases.clear();
				oldbots.erase(found);
			}
			else
			{
				User* bump = ServerInstance->FindNick(nick);
				if (bump)
					bump->ChangeNick(bump->uuid, true);
				std::string uid = ConvToStr(++botID) + "!BOT";
				bot = new ServerBot(uid);
				BotData* bd = new BotData(bot);
				dataExt.set(bot, bd);
				bots.insert(std::make_pair(nick, bd));

				bot->ChangeNick(nick, true);
			}
			bot->ident = tag->getString("ident", "bot");
			bot->host = tag->getString("host", ServerInstance->Config->ServerName);
			bot->dhost = bot->host;
			bot->fullname = tag->getString("name", "Server-side Bot");
			bot->InvalidateCache();
			std::string oper = tag->getString("oper", "Server_Bot");
			if (!oper.empty())
			{
				OperIndex::iterator iter = ServerInstance->Config->oper_blocks.find(" " + oper);
				if (iter != ServerInstance->Config->oper_blocks.end())
					bot->oper = iter->second;
				else
					bot->oper = new OperInfo(oper);
			}
		}
		for(std::map<std::string, BotData*>::iterator i = oldbots.begin(); i != oldbots.end(); i++)
		{
			ServerInstance->GlobalCulls.AddItem(i->second->bot);
		}

		tags = ServerInstance->Config->ConfTags("botcmd");
		for(ConfigIter i = tags.first; i != tags.second; i++)
		{
			ConfigTag* tag = i->second;
			std::string botnick = tag->getString("bot");
			std::map<std::string, BotData*>::iterator found = bots.find(botnick);
			if (found == bots.end())
				continue;
			BotData* bot = found->second;
			Alias a;
			a.AliasedCommand = tag->getString("text").c_str();
			tag->readString("replace", a.ReplaceFormat, true);
			a.RequiredNick = tag->getString("requires");
			a.ULineOnly = tag->getBool("uline");
			a.format = tag->getString("format");

			bot->Aliases.insert(std::make_pair(a.AliasedCommand, a));
		}
 	}

	CullResult cull()
	{
		for(std::map<std::string, BotData*>::iterator i = bots.begin(); i != bots.end(); i++)
		{
			ServerInstance->GlobalCulls.AddItem(i->second->bot);
		}
		return Module::cull();
	}
};

MODULE_INIT(ModuleServerBots)
