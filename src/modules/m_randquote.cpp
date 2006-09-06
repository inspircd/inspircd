/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include "users.h"
#include "channels.h"
#include "modules.h"

#include "inspircd.h"


static FileReader *quotes = NULL;

std::string q_file = "";
std::string prefix = "";
std::string suffix = "";

/* $ModDesc: Provides random Quotes on Connect. */

class cmd_randquote : public command_t
{
 public:
 cmd_randquote (InspIRCd* Instance) : command_t(Instance,"RANDQUOTE", 0, 0)
	{
		this->source = "m_randquote.so";
	}

	CmdResult Handle (const char** parameters, int pcntl, userrec *user)
	{
		std::string str;
		int fsize;
		char buf[MAXBUF];
		if (q_file == "" || quotes->Exists())
		{
			fsize = quotes->FileSize();
			str = quotes->GetLine(rand() % fsize);
			sprintf(buf,"NOTICE %s :%s%s%s",user->nick,prefix.c_str(),str.c_str(),suffix.c_str());
			user->WriteServ(std::string(buf));
		}
		else
		{
			sprintf(buf, "NOTICE %s :Your administrator specified an invalid quotes file, please bug them about this.", user->nick);
			user->WriteServ(std::string(buf));
			return CMD_FAILURE;
		}
		return CMD_SUCCESS;
	}
};

class RandquoteException : public ModuleException
{
 private:
	std::string err;
 public:
	RandquoteException(std::string message) : err(message) { }

	virtual const char* GetReason()
	{
		return (char*)err.c_str();
	}
};

class ModuleRandQuote : public Module
{
 private:
	cmd_randquote* mycommand;
	ConfigReader *conf;
 public:
	ModuleRandQuote(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		conf = new ConfigReader(ServerInstance);
		// Sort the Randomizer thingie..
		srand(time(NULL));

		q_file = conf->ReadValue("randquote","file",0);
		prefix = conf->ReadValue("randquote","prefix",0);
		suffix = conf->ReadValue("randquote","suffix",0);

		mycommand = NULL;

		if (q_file == "")
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
			mycommand = new cmd_randquote(ServerInstance);
			ServerInstance->AddCommand(mycommand);
		}
	}

	void Implements(char* List)
	{
		List[I_OnUserConnect] = 1;
	}
	
	virtual ~ModuleRandQuote()
	{
		DELETE(conf);
		DELETE(quotes);
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}
	
	virtual void OnUserConnect(userrec* user)
	{
		if (mycommand)
			mycommand->Handle(NULL, 0, user);
	}
};


class ModuleRandQuoteFactory : public ModuleFactory
{
 public:
	ModuleRandQuoteFactory()
	{
	}
	
	~ModuleRandQuoteFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleRandQuote(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleRandQuoteFactory;
}
