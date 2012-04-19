/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Changes the ident of connecting bottler clients to 'bottler' */

class ModuleAntiBottler : public Module
{
 public:
	ModuleAntiBottler(InspIRCd* Me)
		: Module(Me)
	{

		Implementation eventlist[] = { I_OnPreCommand };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}



	virtual ~ModuleAntiBottler()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$",VF_VENDOR,API_VERSION);
	}

	virtual int OnPreCommand(std::string &command, std::vector<std::string> &parameters, User *user, bool validated, const std::string &original_line)
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
			if (!*data)
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
				std::vector<std::string> modified;
				modified.push_back("bottler");
				modified.push_back(local);
				modified.push_back(remote);
				modified.push_back(strgecos);
				ServerInstance->Parser->CallHandler("USER", modified, user);
				return 1;
			}
		}
		return 0;
 	}
};

MODULE_INIT(ModuleAntiBottler)
