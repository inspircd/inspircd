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

/* $ModDesc: Provides support for unreal-style channel mode +z and usermode +z for ssl-only queries and notices. */

static char* dummy;

/** Handle channel mode +z
 */
class SSLMode : public ModeHandler
{
 public:
	SSLMode(InspIRCd* Instance) : ModeHandler(Instance, 'z', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool)
	{
		if (adding)
		{
			if (!channel->IsModeSet('z'))
			{
				if (IS_LOCAL(source))
				{
					CUList* userlist = channel->GetUsers();
					for(CUList::iterator i = userlist->begin(); i != userlist->end(); i++)
					{
						if(!i->first->GetExt("ssl", dummy) && !ServerInstance->ULine(i->first->server))
						{
							source->WriteNumeric(ERR_ALLMUSTSSL, "%s %s :all members of the channel must be connected via SSL", source->nick.c_str(), channel->name.c_str());
							return MODEACTION_DENY;
						}
					}
				}
				channel->SetMode('z',true);
				return MODEACTION_ALLOW;
			}
			else
			{
				return MODEACTION_DENY;
			}
		}
		else
		{
			if (channel->IsModeSet('z'))
			{
				channel->SetMode('z',false);
				return MODEACTION_ALLOW;
			}

			return MODEACTION_DENY;
		}
	}
};

class SSLModeUser : public ModeHandler
{
public:
	SSLModeUser(InspIRCd* Instance) : ModeHandler(Instance, 'z', 0, 0, false, MODETYPE_USER, false) { }
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool)
	{
		if (adding)
		{
			if (!dest->IsModeSet('z'))
			{
				dest->SetMode('z', true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('z'))
			{
				dest->SetMode('x',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleSSLModes : public Module
{

	SSLMode* sslm;
	SSLModeUser* sslpm;

 public:
	ModuleSSLModes(InspIRCd* Me)
		: Module(Me)
	{


		sslm = new SSLMode(ServerInstance);
		sslpm = new SSLModeUser(ServerInstance);
		if (!ServerInstance->Modes->AddMode(sslm) || !ServerInstance->Modes->AddMode(sslpm))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnUserPreJoin, I_OnUserPreNotice, I_OnUserPreMessage };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}


	virtual int OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		if(chan && chan->IsModeSet('z'))
		{
			if(user->GetExt("ssl", dummy))
			{
				// Let them in
				return 0;
			}
			else
			{
				// Deny
				user->WriteServ( "489 %s %s :Cannot join channel; SSL users only (+z)", user->nick.c_str(), cname);
				return 1;
			}
		}

		return 0;
	}
	
	virtual int OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (target_type == TYPE_USER)
		{
			User* t = (User*)dest;
			if (t->IsModeSet('z') && !ServerInstance->ULine(user->server))
			{
				if (!user->GetExt("ssl", dummy))
				{
					user->WriteNumeric(ERR_CANTSENDTOUSER, "%s %s :You are not permitted to send private messages to this user (+z set)", user->nick.c_str(), t->nick.c_str());
					return 1;
				}
			}
			else if (user->IsModeSet('z') && !ServerInstance->ULine(t->server))
			{
				if (t->GetExt("ssl", dummy))
				{
					user->WriteNumeric(ERR_CANTSENDTOUSER, "%s %s :You must remove usermode 'z' before you are able to send privates messages to a non-ssl user.", user->nick.c_str(), t->nick.c_str());
					return 1;
				}
			}
		}
		return 0;
	}
	
	virtual int OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (target_type == TYPE_USER)
		{
			User* t = (User*)dest;
			if (t->IsModeSet('z') && !ServerInstance->ULine(user->server))
			{
				if (!user->GetExt("ssl", dummy))
				{
					user->WriteNumeric(ERR_CANTSENDTOUSER, "%s %s :You are not permitted to send private messages to this user (+z set)", user->nick.c_str(), t->nick.c_str());
					return 1;
				}
			}
			else if (user->IsModeSet('z') && !ServerInstance->ULine(t->server))
			{
				if (t->GetExt("ssl", dummy))
				{
					user->WriteNumeric(ERR_CANTSENDTOUSER, "%s %s :You must remove usermode 'z' before you are able to send privates messages to a non-ssl user.", user->nick.c_str(), t->nick.c_str());
					return 1;
				}
			}
		}
		return 0;
	}

	virtual ~ModuleSSLModes()
	{
		ServerInstance->Modes->DelMode(sslm);
		delete sslm;
		delete sslpm;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}
};


MODULE_INIT(ModuleSSLModes)

