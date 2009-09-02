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

/* $ModDesc: Provides the SWHOIS command which allows setting of arbitary WHOIS lines */

/** Handle /SWHOIS
 */
class CommandSwhois : public Command
{

 public:
	CommandSwhois (InspIRCd* Instance, Module* Creator) : Command(Instance, Creator,"SWHOIS","o",2, 2)
	{
		syntax = "<nick> :<swhois>";
		TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User* user)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);

		if (!dest)
		{
			user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}

		std::string* text;
		if (dest->GetExt("swhois", text))
		{
			// We already had it set...
			if (!ServerInstance->ULine(user->server))
				// Ulines set SWHOISes silently
				ServerInstance->SNO->WriteGlobalSno('a', "%s used SWHOIS to set %s's extra whois from '%s' to '%s'", user->nick.c_str(), dest->nick.c_str(), text->c_str(), parameters[1].c_str());

			dest->Shrink("swhois");
			delete text;
		}
		else if (!ServerInstance->ULine(user->server))
		{
			// Ulines set SWHOISes silently
			ServerInstance->SNO->WriteGlobalSno('a', "%s used SWHOIS to set %s's extra whois to '%s'", user->nick.c_str(), dest->nick.c_str(), parameters[1].c_str());
		}

		text = new std::string(parameters[1]);
		dest->Extend("swhois", text);

		/* Bug #376 - feature request -
		 * To cut down on the amount of commands services etc have to recognise, this only sends METADATA across the network now
		 * not an actual SWHOIS command. Any SWHOIS command sent from services will be automatically translated to METADATA by this.
		 * Sorry w00t i know this was your fix, but i got bored and wanted to clear down the tracker :)
		 * -- Brain
		 */
		ServerInstance->PI->SendMetaData(dest, "swhois", *text);

		// If it's an empty swhois, unset it (not ideal, but ok)
		if (text->empty())
		{
			dest->Shrink("swhois");
			delete text;
		}

		return CMD_LOCALONLY;
	}

};

class ModuleSWhois : public Module
{
	CommandSwhois cmd;

	ConfigReader* Conf;

 public:
	ModuleSWhois(InspIRCd* Me) : Module(Me), cmd(Me, this)
	{

		Conf = new ConfigReader(ServerInstance);
		ServerInstance->AddCommand(&cmd);
		Implementation eventlist[] = { I_OnDecodeMetaData, I_OnWhoisLine, I_OnSyncUser, I_OnUserQuit, I_OnCleanup, I_OnRehash, I_OnPostCommand };
		ServerInstance->Modules->Attach(eventlist, this, 7);
	}

	void OnRehash(User* user)
	{
		delete Conf;
		Conf = new ConfigReader(ServerInstance);
	}


	// :kenny.chatspike.net 320 Brain Azhrarn :is getting paid to play games.
	int OnWhoisLine(User* user, User* dest, int &numeric, std::string &text)
	{
		/* We use this and not OnWhois because this triggers for remote, too */
		if (numeric == 312)
		{
			/* Insert our numeric before 312 */
			std::string* swhois;
			if (dest->GetExt("swhois", swhois))
			{
				ServerInstance->SendWhoisLine(user, dest, 320, "%s %s :%s",user->nick.c_str(), dest->nick.c_str(), swhois->c_str());
			}
		}

		/* Dont block anything */
		return 0;
	}

	// Whenever the linking module wants to send out data, but doesnt know what the data
	// represents (e.g. it is metadata, added to a User or Channel by a module) then
	// this method is called. We should use the ProtoSendMetaData function after we've
	// corrected decided how the data should look, to send the metadata on its way if
	// it is ours.
	virtual void OnSyncUser(User* user, Module* proto, void* opaque)
	{
		// check if this user has an swhois field to send
		std::string* swhois;
		if (user->GetExt("swhois", swhois))
			proto->ProtoSendMetaData(opaque,user,"swhois",*swhois);
	}

	// when a user quits, tidy up their metadata
	virtual void OnUserQuit(User* user, const std::string &message, const std::string &oper_message)
	{
		std::string* swhois;
		if (user->GetExt("swhois", swhois))
		{
			user->Shrink("swhois");
			delete swhois;
		}
	}

	// if the module is unloaded, tidy up all our dangling metadata
	virtual void OnCleanup(int target_type, void* item)
	{
		if (target_type != TYPE_USER) return;
		User* user = static_cast<User*>(item);
		std::string* swhois;
		if (user && user->GetExt("swhois", swhois))
		{
			user->Shrink("swhois");
			delete swhois;
		}
	}

	// Whenever the linking module receives metadata from another server and doesnt know what
	// to do with it (of course, hence the 'meta') it calls this method, and it is up to each
	// module in turn to figure out if this metadata key belongs to them, and what they want
	// to do with it.
	// In our case we're only sending a single string around, so we just construct a std::string.
	// Some modules will probably get much more complex and format more detailed structs and classes
	// in a textual way for sending over the link.
	virtual void OnDecodeMetaData(Extensible* target, const std::string &extname, const std::string &extdata)
	{
		User* dest = dynamic_cast<User*>(target);
		// check if its our metadata key, and its associated with a user
		if (extname != "swhois" || !dest)
			return;

		// if they already have an swhois field, trash it and replace it with the remote one.
		std::string* text;
		if (dest->GetExt("swhois", text))
		{
			dest->Shrink("swhois");
			delete text;
		}

		if (extdata.empty())
			return;

		text = new std::string(extdata);
		dest->Extend("swhois", text);
	}

	virtual void OnPostCommand(const std::string &command, const std::vector<std::string> &params, User *user, CmdResult result, const std::string &original_line)
	{
		if ((command != "OPER") || (result != CMD_SUCCESS))
			return;

		std::string swhois;

		for (int i = 0; i < Conf->Enumerate("oper"); i++)
		{
			std::string name = Conf->ReadValue("oper", "name", i);

			if (name == params[0])
			{
				swhois = Conf->ReadValue("oper", "swhois", i);
				break;
			}
		}

		if (!swhois.length())
		{
			for (int i = 0; i < Conf->Enumerate("type"); i++)
			{
				std::string type = Conf->ReadValue("type", "name", i);

				if (type == user->oper)
				{
					swhois = Conf->ReadValue("type", "swhois", i);
					break;
				}
			}
		}

		std::string *old;
		if (user->GetExt("swhois", old))
		{
			user->Shrink("swhois");
			delete old;
		}

		if (!swhois.length())
			return;

		std::string *text = new std::string(swhois);
		user->Extend("swhois", text);
		ServerInstance->PI->SendMetaData(user, "swhois", *text);
	}

	virtual ~ModuleSWhois()
	{
		delete Conf;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleSWhois)
