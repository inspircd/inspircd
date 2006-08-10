/*   +------------------------------------+
 *   | Inspire Internet Relay Chat Daemon |
 *   +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
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
#include "inspircd.h"

// Global Vars
static ConfigReader *helpop;
static Server *Srv;

extern InspIRCd* ServerInstance;

bool do_helpop(const char**, int, userrec*);
void sendtohelpop(userrec*, int, const char**);

/* $ModDesc: /helpop Command, Works like Unreal helpop */

class Helpop : public ModeHandler
{
 public:
	Helpop(InspIRCd* Instance) : ModeHandler(Instance, 'h', 0, 0, false, MODETYPE_USER, true) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!dest->IsModeSet('h'))
			{
				dest->SetMode('h',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('h'))
			{
				dest->SetMode('h',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class cmd_helpop : public command_t
{
 public:
	 cmd_helpop () : command_t("HELPOP",0,1)
	 {
		 this->source = "m_helpop.so";
		 syntax = "[?|!]<any-text>";
	 }

	void Handle (const char** parameters, int pcnt, userrec *user)
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

		if (*parameters[0] == '!')
		{
			// Force send to all +h users
			sendtohelpop(user, pcnt, parameters);
		}
		else if (*parameters[0] == '?')
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
						user->WriteServ("290 "+std::string(user->nick)+" :"+output);
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
						user->WriteServ("290 "+std::string(user->nick)+" :"+output);
					}
	  			}
				// Forward.
				sendtohelpop(user, pcnt, parameters);
			}
		}
	}
};


bool do_helpop(const char** parameters, int pcnt, userrec *src)
{
	char search[MAXBUF];
	std::string output = " "; // a fix bought to you by brain :p
	char a[MAXBUF];
	int nlines = 0;

	if (!pcnt)
	{
 		strcpy(search,"start");
  	}
	else
	{
		if (*parameters[0] == '?')
			parameters[0]++;
 		strlcpy(search,parameters[0],MAXBUF);
   	}

	for (char* n = search; *n; n++)
		*n = tolower(*n);

	for (int i = 1; output != ""; i++)
	{
		snprintf(a,MAXBUF,"line%d",i);
		output = helpop->ReadValue(search, a, 0);
		if (output != "")
		{
			src->WriteServ("290 "+std::string(src->nick)+" :"+output);
			nlines++;
		}
	}
	return (nlines>0);
}



void sendtohelpop(userrec *src, int pcnt, const char **params)
{
	const char* first = params[0];
	if (*first == '!')
	{
		first++;
	}

	std::string line = "*** HELPOPS - From "+std::string(src->nick)+": "+std::string(first)+" ";
	for (int i = 1; i < pcnt; i++)
	{
		line = line + std::string(params[i]) + " ";
	}
	ServerInstance->WriteMode("oh",WM_AND,line.c_str());
}

class HelpopException : public ModuleException
{
 private:
	std::string err;
 public:
	HelpopException(std::string message) : err(message) { }
	virtual const char* GetReason() { return err.c_str(); }
};

class ModuleHelpop : public Module
{
	private:
		ConfigReader *conf;
		std::string  h_file;
		cmd_helpop* mycommand;
		Helpop* ho;

	public:
		ModuleHelpop(InspIRCd* Me)
			: Module::Module(Me)
		{
			ReadConfig();
			ho = new Helpop(ServerInstance);
			Srv->AddMode(ho, 'h');
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
				HelpopException e("Missing helpop file");
				throw(e);
			}

			helpop = new ConfigReader(h_file);
			if ((helpop->ReadValue("nohelp",  "line1", 0) == "") ||
				(helpop->ReadValue("nohelpo", "line1", 0) == "") ||
				(helpop->ReadValue("start",   "line1", 0) == ""))
			{
				HelpopException e("m_helpop: Helpop file is missing important entries. Please check the example conf.");
				throw(e);
			}
		}

		void Implements(char* List)
		{
			List[I_OnRehash] = List[I_OnWhois] = 1;
		}

		virtual void OnRehash(const std::string &parameter)
		{
			DELETE(conf);
			if (helpop)
				DELETE(helpop);

			ReadConfig();
		}

		virtual void OnWhois(userrec* src, userrec* dst)
		{
			if (dst->IsModeSet('h'))
			{
				src->WriteServ("310 "+std::string(src->nick)+" "+std::string(dst->nick)+" :is available for help.");
			}
		}

		virtual ~ModuleHelpop()
		{
			DELETE(conf);
			DELETE(helpop);
			DELETE(ho);
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
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleHelpop(Me);
	}
	
};

extern "C" void * init_module( void )
{
	return new ModuleHelpopFactory;
}
