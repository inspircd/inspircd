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
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Changes the ident of connecting bottler clients to 'bottler' */

class ModuleAntiBottler : public Module
{
 public:
	ModuleAntiBottler(InspIRCd* Me)
		: Module(Me)
	{
		
	}

	void Implements(char* List)
	{
		List[I_OnPreCommand] = 1;
	}

	
	virtual ~ModuleAntiBottler()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}

	virtual int OnPreCommand(const std::string &command, const char** parameters, int pcnt, userrec *user, bool validated, const std::string &original_line)
	{
		char data[MAXBUF];
		strlcpy(data,original_line.c_str(),MAXBUF);
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
				return 0;

			strtok(data," ");
			char *ident = strtok(NULL," ");
			char *local = strtok(NULL," ");
			char *remote = strtok(NULL," :");
			char *gecos = strtok(NULL,"\r\n");

			if (!ident || !local || !remote || !gecos)
				return 0;

			for (char* j = remote; *j; j++)
			{
				if (((*j < '0') || (*j > '9')) && (*j != '.'))
				{
					not_bottler = true;
				}
			}

			if (!not_bottler)
			{
				std::string strgecos = std::string(gecos) + "[Possible bottler, ident: " + std::string(ident) + "]";
				const char* modified[4];
				modified[0] = "bottler";
				modified[1] = local;
				modified[2] = remote;
				modified[3] = strgecos.c_str();
				ServerInstance->Parser->CallHandler("USER", modified, 4, user);
				return 1;
			}
		}
		return 0;
 	}
};

MODULE_INIT(ModuleAntiBottler)
