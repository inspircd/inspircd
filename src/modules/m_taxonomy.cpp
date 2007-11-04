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

/* $ModDesc: Provides the /TAXONOMY command, used to view all metadata attached to a user */

/** Handle /TAXONOMY
 */
class CommandTaxonomy : public Command
{
	Module* Creator;
	bool& claimed;
 public:
	/* Command 'taxonomy', takes no parameters and needs no special modes */
	CommandTaxonomy (InspIRCd* Instance, Module* maker, bool &claim) : Command(Instance,"TAXONOMY", 'o', 1), Creator(maker), claimed(claim)
	{
		this->source = "m_taxonomy.so";
		syntax = "<nickname>";
	}

	CmdResult Handle (const char** parameters, int pcnt, User *user)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);
		if (dest)
		{
			std::deque<std::string> list;
			list.clear();
			user->GetExtList(list);
			user->WriteServ("304 " + std::string(user->nick) + ":TAXONOMY ITEMS " + std::string(dest->nick) + " " +ConvToStr(list.size()));
			for (unsigned int j = 0; j < list.size(); j++)
			{
				claimed = false;
				FOREACH_MOD(I_OnSyncUserMetaData, OnSyncUserMetaData(user, Creator, dest, list[j], true));
				if (!claimed)
				{
					user->WriteServ("304 " + std::string(user->nick) + ":TAXONOMY METADATA " + list[j] + " = <unknown>");
				}
			}
			user->WriteServ("304 " + std::string(user->nick) + ":TAXONOMY END");
		}
		return CMD_LOCALONLY;
	}
};

class ModuleTaxonomy : public Module
{
	CommandTaxonomy* newcommand;
	bool claimed;
 public:
	ModuleTaxonomy(InspIRCd* Me)
		: Module(Me)
	{
		
		// Create a new command
		newcommand = new CommandTaxonomy(ServerInstance, this, claimed);
		ServerInstance->AddCommand(newcommand);
		Implementation eventlist[] = { I_ProtoSendMetaData };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}


	void ProtoSendMetaData(void* opaque, int target_type, void* target, const std::string &extname, const std::string &extdata)
	{
		if (target_type == TYPE_USER)
		{
			User* spool = (User*)opaque;
			std::string taxstr = "304 " + std::string(spool->nick) + ":TAXONOMY METADATA "+extname+" = "+extdata;
			spool->WriteServ(taxstr);
			claimed = true;
		}
	}

	virtual ~ModuleTaxonomy()
	{
	}

	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleTaxonomy)
