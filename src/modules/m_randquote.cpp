/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
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
	CommandRandquote (InspIRCd* Instance) : Command(Instance,"RANDQUOTE", 0, 0)
	{
		this->source = "m_randquote.so";
	}

	CmdResult Handle (const char** parameters, int pcntl, User *user)
	{
		std::string str;
		int fsize;

		if (q_file.empty() || quotes->Exists())
		{
			fsize = quotes->FileSize();
			str = quotes->GetLine(rand() % fsize);
			user->WriteServ("NOTICE %s :%s%s%s",user->nick,prefix.c_str(),str.c_str(),suffix.c_str());
		}
		else
		{
			user->WriteServ("NOTICE %s :Your administrator specified an invalid quotes file, please bug them about this.", user->nick);
			return CMD_FAILURE;
		}

		return CMD_LOCALONLY;
	}
};

/** Thrown by m_randquote
 */
class RandquoteException : public ModuleException
{
 private:
	const std::string err;
 public:
	RandquoteException(const std::string &message) : err(message) { }

	~RandquoteException() throw () { }

	virtual const char* GetReason()
	{
		return err.c_str();
	}
};

class ModuleRandQuote : public Module
{
 private:
	CommandRandquote* mycommand;
	ConfigReader *conf;
 public:
	ModuleRandQuote(InspIRCd* Me)
		: Module(Me)
	{
		
		conf = new ConfigReader(ServerInstance);
		// Sort the Randomizer thingie..
		srand(time(NULL));

		q_file = conf->ReadValue("randquote","file",0);
		prefix = conf->ReadValue("randquote","prefix",0);
		suffix = conf->ReadValue("randquote","suffix",0);

		mycommand = NULL;

		if (q_file.empty())
		{
			RandquoteException e("m_randquote: Quotefile not specified - Please check your config.");
			throw(e);
		}

		quotes = new FileReader(ServerInstance, q_file);
		if(!quotes->Exists())
		{
			RandquoteException e("m_randquote: QuoteFile not Found!! Please check your config - module will not function.");
			throw(e);
		}
		else
		{
			/* Hidden Command -- Mode clients assume /quote sends raw data to an IRCd >:D */
			mycommand = new CommandRandquote(ServerInstance);
			ServerInstance->AddCommand(mycommand);
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
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
	
	virtual void OnUserConnect(User* user)
	{
		if (mycommand)
			mycommand->Handle(NULL, 0, user);
	}
};

MODULE_INIT(ModuleRandQuote)
