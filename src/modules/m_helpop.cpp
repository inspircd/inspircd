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

#include "users.h"
#include "channels.h"
#include "modules.h"

// Global Vars
ConfigReader *helpop;
Server *Srv;

void handle_helpop(char**, int, userrec*);
bool do_helpop(char**, int, userrec*);
void sendtohelpop(userrec*, int, char**);


/* $ModDesc: /helpop Command, Works like Unreal helpop */

void handle_helpop(char **parameters, int pcnt, userrec *user)
{
	char a[MAXBUF];
	std::string output = " ";

	if (pcnt < 1) {
 		do_helpop(NULL,pcnt,user);
		return;
   	}

	if (parameters[0][0] == '!')
	{
		// Force send to all +h users
		sendtohelpop(user, pcnt, parameters);
	} else if (parameters[0][0] == '?') {
		// Force to the helpop system with no forward if not found.
                if (do_helpop(parameters, pcnt, user) == false) {
                        // Not handled by the Database, Tell the user, and forward.
                        for (int i = 1; output != ""; i++)
                        {
                                snprintf(a,MAXBUF,"line%d",i);
                                output = helpop->ReadValue("nohelp", std::string(a), 0);
				if(output != "") {
	                                Srv->SendTo(NULL,user,"290 "+std::string(user->nick)+" :"+output);
				}
                        }
                }
	} else {
		// Check with the helpop database, if not found send to +h
		if (do_helpop(parameters, pcnt, user) == false) {
			// Not handled by the Database, Tell the user, and forward.
		        for (int i = 1; output != ""; i++)
      			{
            			snprintf(a,MAXBUF,"line%d",i);
               			output = helpop->ReadValue("nohelpo", std::string(a), 0);
        			if (output != "") {        		
	                		Srv->SendTo(NULL,user,"290 "+std::string(user->nick)+" :"+output);
				}
      			}
			// Forward.
			sendtohelpop(user, pcnt, parameters);
		}
	}
}

bool do_helpop(char **parameters, int pcnt, userrec *src)
{
	char *search;
	std::string output = " "; // a fix bought to you by brain :p
	char a[MAXBUF];

	if (!parameters) {
 		search = "start";
  	}
	else {
 		search = parameters[0];
   	}

	if (search[0] == '?') {
 		search++;
   	}

        // FIX by brain: make the string lowercase, ConfigReader is
        // case sensitive
        char lower[MAXBUF];
        strlcpy(lower,search,MAXBUF);
        for (int t = 0; t < strlen(lower); t++)
                lower[t] = tolower(lower[t]);


	int nlines = 0;
	for (int i = 1; output != ""; i++)
	{
		snprintf(a,MAXBUF,"line%d",i);
		output = helpop->ReadValue(lower, a, 0);
		if (output != "") {
			Srv->SendTo(NULL,src,"290 "+std::string(src->nick)+" :"+output);
			nlines++;
		}
	}
	return (nlines>0);
}



void sendtohelpop(userrec *src, int pcnt, char **params)
{
	char* first = params[0];
	if (first[0] == '!') { first++; }
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

 public:
	ModuleHelpop()
	{
		Srv  = new Server;
		conf = new ConfigReader;

		h_file = conf->ReadValue("helpop", "file", 0);

		if (h_file == "") {
			printf("m_helpop: Helpop file not Specified.");
			exit(0);
		}

		helpop = new ConfigReader(h_file);

		if ((helpop->ReadValue("nohelp",  "line1", 0) == "") || 
                    (helpop->ReadValue("nohelpo", "line1", 0) == "") ||
                    (helpop->ReadValue("start",   "line1", 0) == ""))
		{
			printf("m_helpop: Helpop file is missing important entries. Please check the example conf.");
			exit(0);
		}

		if (!Srv->AddExtendedMode('h',MT_CLIENT,true,0,0))
		{
			Srv->Log(DEFAULT,"Unable to claim the +h usermode.");
			printf("m_helpop: Unable to claim the +h usermode!");
			exit(0);
		}

		// Loads of comments, untill supported properly.
		Srv->AddCommand("HELPOP",handle_helpop,0,0,"m_helpop.so");

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

	virtual void OnOper(userrec* user)
	{
		char* modes[2];			// only two parameters
		modes[0] = user->nick;		// first parameter is the nick
		modes[1] = "+h";		// second parameter is the mode
		Srv->SendMode(modes,2,user);	// send these, forming the command "MODE <nick> +h"
	}
	
	virtual ~ModuleHelpop()
	{
		delete Srv;
		delete conf;
		delete helpop;
	}
	
	virtual Version GetVersion()
	{
		return Version(0,0,0,1);
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
	
	virtual Module * CreateModule()
	{
		return new ModuleHelpop;
	}
	
};

extern "C" void * init_module( void )
{
	return new ModuleHelpopFactory;
}
