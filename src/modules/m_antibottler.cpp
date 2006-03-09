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

/* $ModDesc: Changes the ident of connecting bottler clients to 'bottler' */

class ModuleAntiBottler : public Module
{
 private:
	 
	 Server *Srv;
 public:
	ModuleAntiBottler(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
	}

        void Implements(char* List)
        {
                List[I_OnServerRaw] = 1;
        }

	
	virtual ~ModuleAntiBottler()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}

	virtual void OnServerRaw(std::string &raw, bool inbound, userrec* user)
	{
		if (inbound)
		{
			char data[MAXBUF];
			strlcpy(data,raw.c_str(),MAXBUF);
			bool not_bottler = false;
			if (!strncmp(data,"user ",5))
			{
				for (char* j = data; *j; j++)
				{
					if (*j == ':')
						break;
						
					if (*j == '"')
					{
						not_bottler = true;
					}
				}
				// Bug Fix (#14) -- FCS

				if (!(data) || !(*data))
					return;

				strtok(data," ");
				char *ident = strtok(NULL," ");
				char *local = strtok(NULL," ");
				char *remote = strtok(NULL," :");
				char *gecos = strtok(NULL,"\r\n");

				if (!ident || !local || !remote || !gecos)
					return;

				for (char* j = remote; *j; j++)
				{
					if (((*j < '0') || (*j > '9')) && (*j != '.'))
					{
						not_bottler = true;
					}
				}

				if (!not_bottler)
				{
					raw = "USER bottler "+std::string(local)+" "+std::string(remote)+" "+std::string(gecos)+" [Possible bottler, ident: "+std::string(ident)+"]";
				}
			}
		}
 	}	
};


class ModuleAntiBottlerFactory : public ModuleFactory
{
 public:
	ModuleAntiBottlerFactory()
	{
	}
	
	~ModuleAntiBottlerFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleAntiBottler(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleAntiBottlerFactory;
}

