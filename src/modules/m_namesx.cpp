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

static const char* dummy = "ON";

/* $ModDesc: Provides aliases of commands. */

class ModuleNamesX : public Module
{
 public:
	
	ModuleNamesX(InspIRCd* Me)
		: Module(Me)
	{
	}

	void Implements(char* List)
	{
		List[I_OnSyncUserMetaData] = List[I_OnPreCommand] = List[I_OnUserList] = List[I_On005Numeric] = 1;
	}

	virtual ~ModuleNamesX()
	{
	}

	void OnSyncUserMetaData(userrec* user, Module* proto,void* opaque, const std::string &extname, bool displayable)
	{
		if ((displayable) && (extname == "NAMESX"))
			proto->ProtoSendMetaData(opaque, TYPE_USER, user, extname, "Enabled");
	}

	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}

	virtual void On005Numeric(std::string &output)
	{
		output.append(" NAMESX");
	}

	virtual int OnPreCommand(const std::string &command, const char** parameters, int pcnt, userrec *user, bool validated, const std::string &original_line)
	{
		irc::string c = command.c_str();
		/* We don't actually create a proper command handler class for PROTOCTL,
		 * because other modules might want to have PROTOCTL hooks too.
		 * Therefore, we just hook its as an unvalidated command therefore we
		 * can capture it even if it doesnt exist! :-)
		 */
		if (c == "PROTOCTL")
		{
			if ((pcnt) && (!strcasecmp(parameters[0],"NAMESX")))
			{
				user->Extend("NAMESX",dummy);
				return 1;
			}
		}
		return 0;
	}

	virtual int OnUserList(userrec* user, chanrec* Ptr, CUList* &ulist)
	{
		if (user->GetExt("NAMESX"))
		{
			char list[MAXBUF];
			size_t dlen, curlen;
			dlen = curlen = snprintf(list,MAXBUF,"353 %s = %s :", user->nick, Ptr->name);
			int numusers = 0;
			char* ptr = list + dlen;

			if (!ulist)
				ulist = Ptr->GetUsers();

			bool has_user = Ptr->HasUser(user);
			for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
			{
				if ((!has_user) && (i->first->IsModeSet('i')))
					continue;

				if (i->first->Visibility && !i->first->Visibility->VisibleTo(user))
					continue;

				size_t ptrlen = snprintf(ptr, MAXBUF, "%s%s ", Ptr->GetAllPrefixChars(i->first), i->second.c_str());
				/* OnUserList can change this - reset it back to normal */
				i->second = i->first->nick;
				curlen += ptrlen;
				ptr += ptrlen;
				numusers++;
				if (curlen > (480-NICKMAX))
				{
					/* list overflowed into multiple numerics */
					user->WriteServ(std::string(list));
					/* reset our lengths */
					dlen = curlen = snprintf(list,MAXBUF,"353 %s = %s :", user->nick, Ptr->name);
					ptr = list + dlen;
					ptrlen = 0;
					numusers = 0;
				}
			}
			/* if whats left in the list isnt empty, send it */
			if (numusers)
			{
				user->WriteServ(std::string(list));
			}
			user->WriteServ("366 %s %s :End of /NAMES list.", user->nick, Ptr->name);
			return 1;
		}
		return 0;		
 	}
};

MODULE_INIT(ModuleNamesX)
