#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Changes the ident of connecting bottler clients to 'bottler' */

class ModuleAntiBottler : public Module
{
 private:
	 
	 Server *Srv;
 public:
	ModuleAntiBottler()
	{
		Srv = new Server;
	}
	
	virtual ~ModuleAntiBottler()
	{
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0);
	}


	virtual void OnServerRaw(std::string &raw, bool inbound, userrec* user)
	{
		if (inbound)
		{
			char data[MAXBUF];
			strncpy(data,raw.c_str(),MAXBUF);
			bool not_bottler = false;
			if (!strncmp(data,"user ",5))
			{
				for (int j = 0; j < strlen(data); j++)
				{
					if (data[j] = ':')
						break;
						
					if (data[j] = '"')
					{
						not_bottler = true;
					}
				}
				// Bug Fix (#14) -- FCS
				if (!strlen(data)) return;				
				char *user = strtok(data," ");
				if (!strlen(data)) return;
				char *ident = strtok(NULL," ");
				if (!strlen(data)) return;
				char *local = strtok(NULL," ");
				if (!strlen(data)) return;
				char *remote = strtok(NULL," :");
				if (!strlen(data)) return;
				char *gecos = strtok(NULL,"\r\n");
				for (int j = 0; j < strlen(remote); j++)
				{
					if (((remote[j] < '0') || (remote[j] > '9')) && (remote[j] != '.'))
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
	
	virtual Module * CreateModule()
	{
		return new ModuleAntiBottler;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleAntiBottlerFactory;
}

