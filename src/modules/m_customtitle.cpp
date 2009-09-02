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

/* $ModDesc: Provides the TITLE command which allows setting of CUSTOM WHOIS TITLE line */

/** Handle /TITLE
 */
class CommandTitle : public Command
{
 public:
	CommandTitle (InspIRCd* Instance, Module* Creator) : Command(Instance, Creator,"TITLE",0,2)
	{
		syntax = "<user> <password>";
		TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
	}

	bool OneOfMatches(const char* host, const char* ip, const char* hostlist)
	{
		std::stringstream hl(hostlist);
		std::string xhost;
		while (hl >> xhost)
		{
			if (InspIRCd::Match(host, xhost, ascii_case_insensitive_map) || InspIRCd::MatchCIDR(ip, xhost, ascii_case_insensitive_map))
			{
				return true;
			}
		}
		return false;
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User* user)
	{
		char TheHost[MAXBUF];
		char TheIP[MAXBUF];

		snprintf(TheHost,MAXBUF,"%s@%s",user->ident.c_str(), user->host.c_str());
		snprintf(TheIP, MAXBUF,"%s@%s",user->ident.c_str(), user->GetIPString());

		ConfigReader Conf(ServerInstance);
		for (int i=0; i<Conf.Enumerate("title"); i++)
		{
			std::string name = Conf.ReadValue("title", "name", "", i);
			std::string pass = Conf.ReadValue("title", "password", "", i);
			std::string hash = Conf.ReadValue("title", "hash", "", i);
			std::string host = Conf.ReadValue("title", "host", "*@*", i);
			std::string title = Conf.ReadValue("title", "title", "", i);
			std::string vhost = Conf.ReadValue("title", "vhost", "", i);

			if (!strcmp(name.c_str(),parameters[0].c_str()) && !ServerInstance->PassCompare(user, pass.c_str(), parameters[1].c_str(), hash.c_str()) && OneOfMatches(TheHost,TheIP,host.c_str()) && !title.empty())
			{
				std::string* text;
				if (user->GetExt("ctitle", text))
				{
					user->Shrink("ctitle");
					delete text;
				}

				text = new std::string(title);
				user->Extend("ctitle", text);

				ServerInstance->PI->SendMetaData(user, "ctitle", *text);

				if (!vhost.empty())
					user->ChangeDisplayedHost(vhost.c_str());

				user->WriteServ("NOTICE %s :Custom title set to '%s'",user->nick.c_str(), title.c_str());

				return CMD_LOCALONLY;
			}
		}

		user->WriteServ("NOTICE %s :Invalid title credentials",user->nick.c_str());
		return CMD_LOCALONLY;
	}

};

class ModuleCustomTitle : public Module
{
	CommandTitle cmd;

 public:
	ModuleCustomTitle(InspIRCd* Me) : Module(Me), cmd(Me, this)
	{
		ServerInstance->AddCommand(&cmd);
		Implementation eventlist[] = { I_OnDecodeMetaData, I_OnWhoisLine, I_OnSyncUser, I_OnUserQuit, I_OnCleanup };
		ServerInstance->Modules->Attach(eventlist, this, 5);
	}


	// :kenny.chatspike.net 320 Brain Azhrarn :is getting paid to play games.
	ModResult OnWhoisLine(User* user, User* dest, int &numeric, std::string &text)
	{
		/* We use this and not OnWhois because this triggers for remote, too */
		if (numeric == 312)
		{
			/* Insert our numeric before 312 */
			std::string* ctitle;
			if (dest->GetExt("ctitle", ctitle))
			{
				ServerInstance->SendWhoisLine(user, dest, 320, "%s %s :%s",user->nick.c_str(), dest->nick.c_str(), ctitle->c_str());
			}
		}
		/* Dont block anything */
		return MOD_RES_PASSTHRU;
	}

	// Whenever the linking module wants to send out data, but doesnt know what the data
	// represents (e.g. it is metadata, added to a User or Channel by a module) then
	// this method is called. We should use the ProtoSendMetaData function after we've
	// corrected decided how the data should look, to send the metadata on its way if
	// it is ours.
	virtual void OnSyncUser(User* user, Module* proto, void* opaque)
	{
		// check if this user has an ctitle field to send
		std::string* ctitle;
		if (user->GetExt("ctitle", ctitle))
			proto->ProtoSendMetaData(opaque,user,"ctitle",*ctitle);
	}

	// when a user quits, tidy up their metadata
	virtual void OnUserQuit(User* user, const std::string &message, const std::string &oper_message)
	{
		std::string* ctitle;
		if (user->GetExt("ctitle", ctitle))
		{
			user->Shrink("ctitle");
			delete ctitle;
		}
	}

	// if the module is unloaded, tidy up all our dangling metadata
	virtual void OnCleanup(int target_type, void* item)
	{
		if (target_type == TYPE_USER)
		{
			User* user = (User*)item;
			std::string* ctitle;
			if (user->GetExt("ctitle", ctitle))
			{
				user->Shrink("ctitle");
				delete ctitle;
			}
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
		if (dest && (extname == "ctitle"))
		{
			std::string* text;
			if (dest->GetExt("ctitle", text))
			{
				dest->Shrink("ctitle");
				delete text;
			}
			if (!extdata.empty())
			{
				text = new std::string(extdata);
				dest->Extend("ctitle", text);
			}
		}
	}

	virtual ~ModuleCustomTitle()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleCustomTitle)
