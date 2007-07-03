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

/* $ModDesc: Sets (and unsets) modes on opers when they oper up */

class ModuleModesOnOper : public Module
{
 private:

	
	ConfigReader *Conf;

 public:
	ModuleModesOnOper(InspIRCd* Me)
		: Module(Me)
	{
		
		Conf = new ConfigReader(ServerInstance);
	}

	void Implements(char* List)
	{
		List[I_OnPostOper] = List[I_OnRehash] = 1;
	}

	virtual void OnRehash(userrec* user, const std::string &parameter)
	{
		DELETE(Conf);
		Conf = new ConfigReader(ServerInstance);
	}
	
	virtual ~ModuleModesOnOper()
	{
		DELETE(Conf);
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
	
	virtual void OnPostOper(userrec* user, const std::string &opertype)
	{
		// whenever a user opers, go through the oper types, find their <type:modes>,
		// and if they have one apply their modes. The mode string can contain +modes
		// to add modes to the user or -modes to take modes from the user.
		for (int j =0; j < Conf->Enumerate("type"); j++)
		{
			std::string typen = Conf->ReadValue("type","name",j);
			if (!strcmp(typen.c_str(),user->oper))
			{
				std::string ThisOpersModes = Conf->ReadValue("type","modes",j);
				if (!ThisOpersModes.empty())
				{
					char first = *(ThisOpersModes.c_str());
					if ((first != '+') && (first != '-'))
						ThisOpersModes = "+" + ThisOpersModes;

					std::string buf;
					stringstream ss(ThisOpersModes);
					vector<string> tokens;

					// split ThisOperModes into modes and mode params
					while (ss >> buf)
						tokens.push_back(buf);

					int size = tokens.size() + 1;
					const char** modes = new const char*[size];
					modes[0] = user->nick;

					// process mode params
					int i = 1;
					for (unsigned int k = 0; k < tokens.size(); k++)
					{
						modes[i] = tokens[k].c_str();
						i++;
					}

					std::deque<std::string> n;
					Event rmode((char *)&n, NULL, "send_mode_explicit");
					for (unsigned int j = 0; j < tokens.size(); j++)
						n.push_back(modes[j]);

					rmode.Send(ServerInstance);
					ServerInstance->SendMode(modes, size, user);
					delete [] modes;
				}
				break;
			}
		}
	}
};

MODULE_INIT(ModuleModesOnOper)
