#include <stdio.h>
#include <stdlib.h>
#include <fstream>

#include "users.h"
#include "channels.h"
#include "modules.h"


/* $ModDesc: Provides random Quotes on Connect. */

class ModuleRandQuote : public Module
{
 private:

	 Server *Srv;
	 ConfigReader *conf;
	 FileReader *quotes;

	 std::string q_file;
	 std::string prefix;
	 std::string suffix;
	 
 public:
	ModuleRandQuote()
	{
		Srv = new Server;
		conf = new ConfigReader;


		q_file = conf->ReadValue("randquote","file",0);
		prefix = conf->ReadValue("randquote","prefix",0);
		suffix = conf->ReadValue("randquote","suffix",0);

		if (q_file == "") {
			printf("m_randquote: Quotefile not specified.. Please check your config.\n\n");
			exit(0);
                }


		quotes = new FileReader(q_file);
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
		std::string str;
		int fsize;
		char buf[MAXBUF];

		fsize = quotes->FileSize();
		srand(time(NULL));
		str = quotes->GetLine(rand() % fsize);
			
		sprintf(buf,"NOTICE %s :%s%s%s",user->nick,prefix.c_str(),str.c_str(),suffix.c_str());
		Srv->SendServ(user->fd, buf);
		return;
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

