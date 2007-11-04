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

static const char* dummy = "ON";

/* $ModDesc: Provides aliases of commands. */

class ModuleUHNames : public Module
{
	CUList nl;
 public:
	
	ModuleUHNames(InspIRCd* Me)
		: Module(Me)
	{
		Implementation eventlist[] = { I_OnSyncUserMetaData, I_OnPreCommand, I_OnUserList, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 4);
	}


	virtual ~ModuleUHNames()
	{
	}

	void OnSyncUserMetaData(User* user, Module* proto,void* opaque, const std::string &extname, bool displayable)
	{
		if ((displayable) && (extname == "UHNAMES"))
			proto->ProtoSendMetaData(opaque, TYPE_USER, user, extname, "Enabled");
	}

	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}

	virtual void On005Numeric(std::string &output)
	{
		output.append(" UHNAMES");
	}

	void Prioritize()
	{
		Module* namesx = ServerInstance->Modules->Find("m_namesx.so");
		ServerInstance->Modules->SetPriority(this, I_OnUserList, PRIO_BEFORE, &namesx);
	}

	virtual int OnPreCommand(const std::string &command, const char** parameters, int pcnt, User *user, bool validated, const std::string &original_line)
	{
		irc::string c = command.c_str();
		/* We don't actually create a proper command handler class for PROTOCTL,
		 * because other modules might want to have PROTOCTL hooks too.
		 * Therefore, we just hook its as an unvalidated command therefore we
		 * can capture it even if it doesnt exist! :-)
		 */
		if (c == "PROTOCTL")
		{
			if ((pcnt) && (!strcasecmp(parameters[0],"UHNAMES")))
			{
				user->Extend("UHNAMES",dummy);
				return 1;
			}
		}
		return 0;
	}

	/* IMPORTANT: This must be prioritized above NAMESX! */
	virtual int OnUserList(User* user, Channel* Ptr, CUList* &ulist)
	{
		if (user->GetExt("UHNAMES"))
		{
			if (!ulist)
				ulist = Ptr->GetUsers();

			for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
				i->second = i->first->GetFullHost();
		}
		return 0;		
 	}
};

MODULE_INIT(ModuleUHNames)
