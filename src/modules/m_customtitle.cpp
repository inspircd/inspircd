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
#include "wildcard.h"

/* $ModDesc: Provides the TITLE command which allows setting of CUSTOM WHOIS TITLE line */

/** Handle /TITLE
 */
class CommandTitle : public Command
{
	
 public:
	CommandTitle (InspIRCd* Instance) : Command(Instance,"TITLE",0,2)
	{
		this->source = "m_customtitle.so";
		syntax = "<user> <password>";
		TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
	}

bool OneOfMatches(const char* host, const char* ip, const char* hostlist)
{
    std::stringstream hl(hostlist);
    std::string xhost;
    while (hl >> xhost)
    {
        if (match(host,xhost.c_str()) || match(ip,xhost.c_str(),true))
        {
            return true;
        }
    }
    return false;
}

	CmdResult Handle(const char** parameters, int pcnt, User* user)
	{
		if (!IS_LOCAL(user))
			return CMD_LOCALONLY;
	
		char TheHost[MAXBUF];
		char TheIP[MAXBUF];

		snprintf(TheHost,MAXBUF,"%s@%s",user->ident,user->host);
		snprintf(TheIP, MAXBUF,"%s@%s",user->ident,user->GetIPString());

		ConfigReader Conf(ServerInstance);
		for (int i=0; i<Conf.Enumerate("title"); i++)
		{
			std::string name = Conf.ReadValue("title", "name", "", i);
			std::string pass = Conf.ReadValue("title", "password", "", i);
			std::string host = Conf.ReadValue("title", "host", "*@*", i);
			std::string title = Conf.ReadValue("title", "title", "", i);
			std::string vhost = Conf.ReadValue("title", "vhost", "", i);

			if (!strcmp(name.c_str(),parameters[0]) && !strcmp(pass.c_str(),parameters[1]) && OneOfMatches(TheHost,TheIP,host.c_str()) && !title.empty())
			{
				std::string* text;
				user->GetExt("ctitle", text);

				if (text)
				{
					user->Shrink("ctitle");
					delete text;
				}

				text = new std::string(title);
				user->Extend("ctitle", text);

				std::deque<std::string>* metadata = new std::deque<std::string>;
				metadata->push_back(user->nick);
				metadata->push_back("ctitle");      // The metadata id
				metadata->push_back(*text);     // The value to send
				Event event((char*)metadata,(Module*)this,"send_metadata");
				event.Send(ServerInstance);
				delete metadata;
				                                                        
				if (!vhost.empty())
					user->ChangeDisplayedHost(vhost.c_str());

				if (!ServerInstance->ULine(user->server))
					// Ulines set TITLEs silently
					ServerInstance->WriteOpers("*** %s used TITLE to set custom title '%s'",user->nick,title.c_str());

				user->WriteServ("NOTICE %s :Custom title set to '%s'",user->nick, title.c_str());

				return CMD_SUCCESS;
			}
		}

		if (!ServerInstance->ULine(user->server))
			// Ulines also fail TITLEs silently
			ServerInstance->WriteOpers("*** Failed TITLE attempt by %s!%s@%s using login '%s'",user->nick,user->ident,user->host,parameters[0]);

		user->WriteServ("NOTICE %s :Invalid title credentials",user->nick);
		return CMD_SUCCESS;
	}

};

class ModuleCustomTitle : public Module
{
	CommandTitle* mycommand;
	
 public:
	ModuleCustomTitle(InspIRCd* Me) : Module(Me)
	{
		
		mycommand = new CommandTitle(ServerInstance);
		ServerInstance->AddCommand(mycommand);
		Implementation eventlist[] = { I_OnDecodeMetaData, I_OnWhoisLine, I_OnSyncUserMetaData, I_OnUserQuit, I_OnCleanup };
		ServerInstance->Modules->Attach(eventlist, this, 5);
	}


	// :kenny.chatspike.net 320 Brain Azhrarn :is getting paid to play games.
	int OnWhoisLine(User* user, User* dest, int &numeric, std::string &text)
	{
		/* We use this and not OnWhois because this triggers for remote, too */
		if (numeric == 312)
		{
			/* Insert our numeric before 312 */
			std::string* ctitle;
			dest->GetExt("ctitle", ctitle);
			if (ctitle)
			{
				ServerInstance->SendWhoisLine(user, dest, 320, "%s %s :%s",user->nick,dest->nick,ctitle->c_str());
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
	virtual void OnSyncUserMetaData(User* user, Module* proto, void* opaque, const std::string &extname, bool displayable)
	{
		// check if the linking module wants to know about OUR metadata
		if (extname == "ctitle")
		{
			// check if this user has an ctitle field to send
			std::string* ctitle;
			user->GetExt("ctitle", ctitle);
			if (ctitle)
			{
				// call this function in the linking module, let it format the data how it
				// sees fit, and send it on its way. We dont need or want to know how.
				proto->ProtoSendMetaData(opaque,TYPE_USER,user,extname,*ctitle);
			}
		}
	}

	// when a user quits, tidy up their metadata
	virtual void OnUserQuit(User* user, const std::string &message, const std::string &oper_message)
	{
		std::string* ctitle;
		user->GetExt("ctitle", ctitle);
		if (ctitle)
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
			user->GetExt("ctitle", ctitle);
			if (ctitle)
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
	virtual void OnDecodeMetaData(int target_type, void* target, const std::string &extname, const std::string &extdata)
	{
		// check if its our metadata key, and its associated with a user
		if ((target_type == TYPE_USER) && (extname == "ctitle"))
		{
			User* dest = (User*)target;
			// if they dont already have an ctitle field, accept the remote server's
			std::string* text;
			if (!dest->GetExt("ctitle", text))
			{
				std::string* text = new std::string(extdata);
				dest->Extend("ctitle",text);
			}
		}
	}
	
	virtual ~ModuleCustomTitle()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_COMMON | VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleCustomTitle)
