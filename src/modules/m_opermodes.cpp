/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

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
		Implementation eventlist[] = { I_OnPostOper, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}


	virtual void OnRehash(User* user)
	{
		delete Conf;
		Conf = new ConfigReader(ServerInstance);
	}

	virtual ~ModuleModesOnOper()
	{
		delete Conf;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}

	virtual void OnPostOper(User* user, const std::string &opertype, const std::string &opername)
	{
		// whenever a user opers, go through the oper types, find their <type:modes>,
		// and if they have one apply their modes. The mode string can contain +modes
		// to add modes to the user or -modes to take modes from the user.
		for (int j =0; j < Conf->Enumerate("type"); j++)
		{
			std::string typen = Conf->ReadValue("type","name",j);
			if (typen == user->oper)
			{
				std::string ThisOpersModes = Conf->ReadValue("type","modes",j);
				if (!ThisOpersModes.empty())
				{
					ApplyModes(user, ThisOpersModes);
				}
				break;
			}
		}

		if (!opername.empty()) // if user is local ..
		{
			for (int j = 0; j < Conf->Enumerate("oper"); j++)
			{
				if (opername == Conf->ReadValue("oper", "name", j))
				{
					std::string ThisOpersModes = Conf->ReadValue("oper", "modes", j);
					if (!ThisOpersModes.empty())
					{
						ApplyModes(user, ThisOpersModes);
					}
					break;
				}
			}
		}
	}

	void ApplyModes(User *u, std::string &smodes)
	{
		char first = *(smodes.c_str());
		if ((first != '+') && (first != '-'))
			smodes = "+" + smodes;

		std::string buf;
		std::stringstream ss(smodes);
		std::vector<std::string> tokens;

		// split into modes and mode params
		while (ss >> buf)
			tokens.push_back(buf);

		std::vector<std::string> modes;
		modes.push_back(u->nick);

		// process mode params
		for (unsigned int k = 0; k < tokens.size(); k++)
		{
			modes.push_back(tokens[k]);
		}

		ServerInstance->SendMode(modes, u);
		ServerInstance->PI->SendMode(u->uuid, ServerInstance->Modes->GetLastParseParams(), ServerInstance->Modes->GetLastParseTranslate());
	}
};

MODULE_INIT(ModuleModesOnOper)
