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

static FileReader *quotes = NULL;

std::string q_file;
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

		if (q_file.empty() || quotes->Exists())
		{
			fsize = quotes->FileSize();
			str = quotes->GetLine(rand() % fsize);
			user->WriteServ("NOTICE %s :%s%s%s",user->nick.c_str(),prefix.c_str(),str.c_str(),suffix.c_str());
		}
		else
		{
			user->WriteServ("NOTICE %s :Your administrator specified an invalid quotes file, please bug them about this.", user->nick.c_str());
			return CMD_FAILURE;
		}

		return CMD_SUCCESS;
	}
};

class ModuleRandQuote : public Module
{
 private:
	CommandRandquote cmd;
	ConfigReader *conf;
 public:
	ModuleRandQuote(InspIRCd* Me)
		: Module(Me), cmd(this)
	{

		conf = new ConfigReader(ServerInstance);
		// Sort the Randomizer thingie..
		srand(ServerInstance->Time());

		q_file = conf->ReadValue("randquote","file",0);
		prefix = conf->ReadValue("randquote","prefix",0);
		suffix = conf->ReadValue("randquote","suffix",0);

		if (q_file.empty())
		{
			throw ModuleException("m_randquote: Quotefile not specified - Please check your config.");
		}

		quotes = new FileReader(ServerInstance, q_file);
		if(!quotes->Exists())
		{
			throw ModuleException("m_randquote: QuoteFile not Found!! Please check your config - module will not function.");
		}
		else
		{
			/* Hidden Command -- Mode clients assume /quote sends raw data to an IRCd >:D */
			ServerInstance->AddCommand(&cmd);
		}
		Implementation eventlist[] = { I_OnUserConnect };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}


	virtual ~ModuleRandQuote()
	{
		delete conf;
		delete quotes;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$",VF_VENDOR,API_VERSION);
	}

	virtual void OnUserConnect(User* user)
	{
		cmd.Handle(std::vector<std::string>(), user);
	}
};

MODULE_INIT(ModuleRandQuote)
