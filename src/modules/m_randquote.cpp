/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
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

#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include "users.h"
#include "channels.h"
#include "modules.h"

Server *Srv;
FileReader *quotes;

std::string q_file;
std::string prefix;
std::string suffix;



/* $ModDesc: Provides random Quotes on Connect. */

void handle_randquote(char** parameters, int pcntl, userrec *user)
{
	std::string str;
	int fsize;
	char buf[MAXBUF];

	fsize = quotes->FileSize();
	str = quotes->GetLine(rand() % fsize);

	sprintf(buf,"NOTICE %s :%s%s%s",user->nick,prefix.c_str(),str.c_str(),suffix.c_str());
	Srv->SendServ(user->fd, buf);
	return;
}




class ModuleRandQuote : public Module
{
 private:

	ConfigReader *conf;
 public:
	ModuleRandQuote()
	{
		Srv = new Server;
		conf = new ConfigReader;
		// Sort the Randomizer thingie..
		srand(time(NULL));


		q_file = conf->ReadValue("randquote","file",0);
		prefix = conf->ReadValue("randquote","prefix",0);
		suffix = conf->ReadValue("randquote","suffix",0);

		if (q_file == "") {
			printf("m_randquote: Quotefile not specified.. Please check your config.\n\n");
			exit(0);
                }


		quotes = new FileReader(q_file);
		if(!quotes->Exists())
		{
			printf("m_randquote: QuoteFile not Found!! Please check your config.\n\n");
			exit(0);
		}
		/* Hidden Command -- Mode clients assume /quote sends raw data to an IRCd >:D */
		Srv->AddCommand("QUOTE",handle_randquote,0,0,"m_randquote.so");
	}
	
	virtual ~ModuleRandQuote()
	{
		delete Srv;
		delete conf;
		delete quotes;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0);
	}
	
	virtual void OnUserConnect(userrec* user)
	{
		// Make a fake pointer to be passed to handle_randquote()
		// Dont try this at home kiddies :D
		char *rar = "RAR";
		handle_randquote(&rar, 0, user);
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
	
	virtual Module * CreateModule()
	{
		return new ModuleRandQuote;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleRandQuoteFactory;
}
