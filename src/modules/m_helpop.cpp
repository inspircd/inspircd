/*   +------------------------------------+
 *   | Inspire Internet Relay Chat Daemon |
 *   +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
 *   E-mail:
 *<brain@chatspike.net>
 *   	  <Craig@chatspike.net>
 * 
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"

// Global Vars
/* XXX - should all this be marked static? clear the global namespace, etc. */
ConfigReader *helpop;
Server *Srv;

void handle_helpop(char**, int, userrec*);
bool do_helpop(char**, int, userrec*);
void sendtohelpop(userrec*, int, char**);

/* $ModDesc: /helpop Command, Works like Unreal helpop */

class cmd_helpop : public command_t
{
 public:
	 cmd_helpop () : command_t("HELPOP",0,0)
	 {
		 this->source = "m_helpop.so";
	 }

	void Handle (char **parameters, int pcnt, userrec *user)
	{
		char a[MAXBUF];
		std::string output = " ";

		if (!helpop)
			return;

		if (pcnt < 1)
		{
	 		do_helpop(NULL,pcnt,user);
			return;
	   	}

		if (parameters[0][0] == '!')
		{
			// Force send to all +h users
			sendtohelpop(user, pcnt, parameters);
		}
		else if (parameters[0][0] == '?')
		{
			// Force to the helpop system with no forward if not found.
			if (do_helpop(parameters, pcnt, user) == false)
			{
				// Not handled by the Database, Tell the user, and bail.
				for (int i = 1; output != ""; i++)
				{
					snprintf(a,MAXBUF,"line%d",i);
					output = helpop->ReadValue("nohelp", std::string(a), 0);
	
					if(output != "")
					{
						Srv->SendTo(NULL,user,"290 "+std::string(user->nick)+" :"+output);
					}
				}
			}
		}
		else
		{
			// Check with the helpop database, if not found send to +h
			if (do_helpop(parameters, pcnt, user) == false)
			{
				// Not handled by the Database, Tell the user, and forward.
				for (int i = 1; output != ""; i++)
	  			{
					snprintf(a,MAXBUF,"line%d",i);
					/* "nohelpo" for opers "nohelp" for users */
	   				output = helpop->ReadValue("nohelpo", std::string(a), 0);
					if (output != "")
					{
						Srv->SendTo(NULL,user,"290 "+std::string(user->nick)+" :"+output);
					}
	  			}
				// Forward.
				sendtohelpop(user, pcnt, parameters);
			}
		}
	}
};


bool do_helpop(char **parameters, int pcnt, userrec *src)
{
	char *search;
	std::string output = " "; // a fix bought to you by brain :p
	char a[MAXBUF];
	char lower[MAXBUF];
	int nlines = 0;

	if (!parameters)
	{
 		search = "start";
  	}
	else
	{
 		search = parameters[0];
   	}

	if (search[0] == '?')
	{
 		search++;
   	}

	/* XXX - don't we have an strtolower()? if not, might pay to add one.. that works on char *, preferably.. */
	strlcpy(lower, search, MAXBUF);
	for (unsigned int t = 0; t < strlen(lower); t++)
		lower[t] = tolower(lower[t]);


	for (int i = 1; output != ""; i++)
	{
		snprintf(a,MAXBUF,"line%d",i);
		output = helpop->ReadValue(lower, a, 0);
		if (output != "")
		{
			Srv->SendTo(NULL,src,"290 "+std::string(src->nick)+" :"+output);
			nlines++;
		}
	}
	return (nlines>0);
}



void sendtohelpop(userrec *src, int pcnt, char **params)
{
	char* first = params[0];
	if (*first == '!')
	{
		first++;
	}

	std::string line = "*** HELPOPS - From "+std::string(src->nick)+": "+std::string(first)+" ";
	for (int i = 1; i < pcnt; i++)
	{
		line = line + std::string(params[i]) + " ";
	}
	Srv->SendToModeMask("oh",WM_AND,line);
}

class ModuleHelpop : public Module
{
	private:
		ConfigReader *conf;
		std::string  h_file;
		cmd_helpop* mycommand;

	public:
		ModuleHelpop(Server* Me)
			: Module::Module(Me)
		{
			Srv  = Me;

			ReadConfig();
			if (!Srv->AddExtendedMode('h',MT_CLIENT,true,0,0))
			{
				Srv->Log(DEFAULT,"Unable to claim the +h usermode.");
				return;
			}

			mycommand = new cmd_helpop();
			Srv->AddCommand(mycommand);
		}

		virtual void ReadConfig()
		{
			conf = new ConfigReader;
			h_file = conf->ReadValue("helpop", "file", 0);

			if (h_file == "")
			{
				helpop = NULL;
				log(DEFAULT,"m_helpop: Helpop file not Specified.");
				return;
			}

			helpop = new ConfigReader(h_file);
			if ((helpop->ReadValue("nohelp",  "line1", 0) == "") ||
				(helpop->ReadValue("nohelpo", "line1", 0) == "") ||
				(helpop->ReadValue("start",   "line1", 0) == ""))
			{
				log(DEFAULT,"m_helpop: Helpop file is missing important entries. Please check the example conf.");
				return;
			}
		}


		virtual void OnRehash(std::string parameter)
		{
			delete conf;
			if (helpop)
				delete helpop;

			ReadConfig();
		}

		virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
		{
			if ((modechar == 'h') && (type == MT_CLIENT))
	  		{
				return 1;
			}
			return 0;
		}

		virtual void OnWhois(userrec* src, userrec* dst)
		{
			if (strchr(dst->modes,'h'))
			{
				Srv->SendTo(NULL,src,"310 "+std::string(src->nick)+" "+std::string(dst->nick)+" :is available for help.");
			}
		}

		virtual ~ModuleHelpop()
		{
			delete conf;
			delete helpop;
		}
	
		virtual Version GetVersion()
		{
			return Version(1,0,0,1,VF_STATIC|VF_VENDOR);
		}
};

class ModuleHelpopFactory : public ModuleFactory
{
 public:
	ModuleHelpopFactory()
	{
	}
	
	~ModuleHelpopFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleHelpop(Me);
	}
	
};

extern "C" void * init_module( void )
{
	return new ModuleHelpopFactory;
}
