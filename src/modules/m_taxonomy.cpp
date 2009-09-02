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

/* $ModDesc: Provides the /TAXONOMY command, used to view all metadata attached to a user */

/** Handle /TAXONOMY
 */
class CommandTaxonomy : public Command
{
	Module* Creator;
 public:
	/* Command 'taxonomy', takes no parameters and needs no special modes */
	CommandTaxonomy (InspIRCd* Instance, Module* maker) : Command(Instance,"TAXONOMY", "o", 1), Creator(maker)
	{
		this->source = "m_taxonomy.so";
		syntax = "<nickname>";
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);
		if (dest)
		{
			std::deque<std::string> list;
			dest->GetExtList(list);
			user->WriteNumeric(304, "" + std::string(user->nick) + ":TAXONOMY ITEMS " + std::string(dest->nick) + " " +ConvToStr(list.size()));
			FOREACH_MOD(I_OnSyncUser, OnSyncUser(dest, Creator, user));
			user->WriteNumeric(304, "" + std::string(user->nick) + ":TAXONOMY END");
		}
		return CMD_LOCALONLY;
	}
};

class ModuleTaxonomy : public Module
{
	CommandTaxonomy cmd;
 public:
	ModuleTaxonomy(InspIRCd* Me)
		: Module(Me), cmd(Me, this)
	{
		ServerInstance->AddCommand(&cmd);
	}


	void ProtoSendMetaData(void* opaque, Extensible* target, const std::string &extname, const std::string &extdata)
	{
		User* spoolto = (User*)opaque;
		std::string taxstr = "304 " + std::string(spoolto->nick) + ":TAXONOMY METADATA "+extname+" = "+extdata;
		spoolto->WriteServ(taxstr);
	}

	virtual ~ModuleTaxonomy()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}

	virtual std::string ProtoTranslate(Extensible* item)
	{
		User* u = dynamic_cast<User*>(item);
		Channel* c = dynamic_cast<Channel*>(item);
		if (u)
			return u->nick;
		if (c)
			return c->name;
		return "?";
	}
};

MODULE_INIT(ModuleTaxonomy)
