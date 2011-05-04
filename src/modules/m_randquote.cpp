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

static FileReader *quotes = NULL;

std::string prefix;
std::string suffix;

/* $ModDesc: Provides random Quotes on Connect. */

/** Handle /RANDQUOTE
 */
class CommandRandquote : public Command
{
 public:
	CommandRandquote(Module* Creator) : Command(Creator,"RANDQUOTE", 0)
	{
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		std::string str;
		int fsize;

		fsize = quotes->FileSize();
		str = quotes->GetLine(ServerInstance->GenRandomInt(fsize));
		user->WriteServ("NOTICE %s :%s%s%s",user->nick.c_str(),prefix.c_str(),str.c_str(),suffix.c_str());

		return CMD_SUCCESS;
	}
};

class ModuleRandQuote : public Module
{
 private:
	CommandRandquote cmd;
 public:
	ModuleRandQuote()
		: cmd(this)
	{
	}

	void init()
	{
		ConfigTag* conf = ServerInstance->Config->GetTag("randquote");

		std::string q_file = conf->getString("file","quotes");
		prefix = conf->getString("prefix");
		suffix = conf->getString("suffix");

		quotes = new FileReader(q_file);
		if (!quotes->Exists())
		{
			throw ModuleException("m_randquote: QuoteFile not Found!! Please check your config - module will not function.");
		}
		ServerInstance->AddCommand(&cmd);
		Implementation eventlist[] = { I_OnUserConnect };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}


	virtual ~ModuleRandQuote()
	{
		delete quotes;
	}

	virtual Version GetVersion()
	{
		return Version("Provides random Quotes on Connect.",VF_VENDOR);
	}

	virtual void OnUserConnect(LocalUser* user)
	{
		cmd.Handle(std::vector<std::string>(), user);
	}
};

MODULE_INIT(ModuleRandQuote)
