/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
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
 public:
	void init()
	{
		Implementation eventlist[] = { I_OnPostOper };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}


	void ReadConfig(ConfigReadStatus&)
	{
	}

	virtual ~ModuleModesOnOper()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Sets (and unsets) modes on opers when they oper up", VF_VENDOR);
	}

	virtual void OnPostOper(User* user, const std::string &opertype, const std::string &opername)
	{
		// whenever a user opers, go through the oper types, find their <type:modes>,
		// and if they have one apply their modes. The mode string can contain +modes
		// to add modes to the user or -modes to take modes from the user.
		std::string ThisOpersModes = user->oper->getConfig("automodes");
		if (!ThisOpersModes.empty())
		{
			ApplyModes(user, ThisOpersModes);
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

		ServerInstance->SendGlobalMode(modes, u);
	}
};

MODULE_INIT(ModuleModesOnOper)
