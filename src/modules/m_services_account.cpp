/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include "users.h"
#include "channels.h"
#include "modules.h"
#include "hashcomp.h"
#include "inspircd.h"

/* $ModDesc: Povides support for ircu-style services accounts, including chmode +R, etc. */

class AChannel_R : public ModeHandler
{
 public:
	AChannel_R(InspIRCd* Instance) : ModeHandler(Instance, 'R', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('R'))
			{
				channel->SetMode('R',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('R'))
			{
				channel->SetMode('R',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class AUser_R : public ModeHandler
{
 public:
	AUser_R(InspIRCd* Instance) : ModeHandler(Instance, 'R', 0, 0, false, MODETYPE_USER, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!dest->IsModeSet('R'))
			{
				dest->SetMode('R',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('R'))
			{
				dest->SetMode('R',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class AChannel_M : public ModeHandler
{
 public:
	AChannel_M(InspIRCd* Instance) : ModeHandler(Instance, 'M', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('M'))
			{
				channel->SetMode('M',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('M'))
			{
				channel->SetMode('M',true);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleServicesAccount : public Module
{
	 
	AChannel_R* m1;
	AChannel_M* m2;
	AUser_R* m3;
 public:
	ModuleServicesAccount(InspIRCd* Me) : Module::Module(Me)
	{
		
		m1 = new AChannel_R(ServerInstance);
		m2 = new AChannel_M(ServerInstance);
		m3 = new AUser_R(ServerInstance);
		ServerInstance->AddMode(m1, 'R');
		ServerInstance->AddMode(m2, 'M');
		ServerInstance->AddMode(m3, 'R');
	}

	/* <- :twisted.oscnet.org 330 w00t2 w00t2 w00t :is logged in as */
	virtual void OnWhois(userrec* source, userrec* dest)
	{
		std::string *account;
		dest->GetExt("accountname", account);

		if (account)
		{
			source->WriteServ("330 %s %s %s :is logged in as", source->nick, dest->nick, account->c_str());
		}
	}

	void Implements(char* List)
	{
		List[I_OnWhois] = List[I_OnUserPreMessage] = List[I_OnUserPreNotice] = List[I_OnUserPreJoin] = 1;
		List[I_OnSyncUserMetaData] = List[I_OnUserQuit] = List[I_OnCleanup] = List[I_OnDecodeMetaData] = 1;
	}

	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		std::string *account;
		user->GetExt("accountname", account);
		
		if (target_type == TYPE_CHANNEL)
		{
			chanrec* c = (chanrec*)dest;
			
			if ((c->IsModeSet('M')) && (!account))
			{
				if ((ServerInstance->ULine(user->nick)) || (ServerInstance->ULine(user->server)) || (!strcmp(user->server,"")))
				{
					// user is ulined, can speak regardless
					return 0;
				}

				// user messaging a +M channel and is not registered
				user->WriteServ("477 "+std::string(user->nick)+" "+std::string(c->name)+" :You need to be identified to a registered account to message this channel");
				return 1;
			}
		}
		if (target_type == TYPE_USER)
		{
			userrec* u = (userrec*)dest;
			
			if ((u->modes['R'-65]) && (!account))
			{
				if ((ServerInstance->ULine(user->nick)) || (ServerInstance->ULine(user->server)))
				{
					// user is ulined, can speak regardless
					return 0;
				}

				// user messaging a +R user and is not registered
				user->WriteServ("477 "+std::string(user->nick)+" "+std::string(u->nick)+" :You need to be identified to a registered account to message this user");
				return 1;
			}
		}
		return 0;
	}
	 
	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		return OnUserPreMessage(user, dest, target_type, text, status);
	}
	 
	virtual int OnUserPreJoin(userrec* user, chanrec* chan, const char* cname)
	{
		std::string *account;
		user->GetExt("accountname", account);
		
		if (chan)
		{
			if (chan->IsModeSet('R'))
			{
				if (!account)
				{
					if ((ServerInstance->ULine(user->nick)) || (ServerInstance->ULine(user->server)))
					{
						// user is ulined, won't be stopped from joining
						return 0;
					}
					// joining a +R channel and not identified
					user->WriteServ("477 "+std::string(user->nick)+" "+std::string(chan->name)+" :You need to be identified to a registered account to join this channel");
					return 1;
				}
			}
		}
		return 0;
	}
	
	// Whenever the linking module wants to send out data, but doesnt know what the data
	// represents (e.g. it is metadata, added to a userrec or chanrec by a module) then
	// this method is called. We should use the ProtoSendMetaData function after we've
	// corrected decided how the data should look, to send the metadata on its way if
	// it is ours.
	virtual void OnSyncUserMetaData(userrec* user, Module* proto, void* opaque, const std::string &extname)
	{
		// check if the linking module wants to know about OUR metadata
		if (extname == "accountname")
		{
			// check if this user has an swhois field to send
			std::string* account;
			user->GetExt("accountname", account);
			if (account)
			{
				// call this function in the linking module, let it format the data how it
				// sees fit, and send it on its way. We dont need or want to know how.
				proto->ProtoSendMetaData(opaque,TYPE_USER,user,extname,*account);
			}
		}
	}

	// when a user quits, tidy up their metadata
	virtual void OnUserQuit(userrec* user, const std::string &message)
	{
		std::string* account;
		user->GetExt("accountname", account);
		if (account)
		{
			user->Shrink("accountname");
			delete account;
		}
	}

	// if the module is unloaded, tidy up all our dangling metadata
	virtual void OnCleanup(int target_type, void* item)
	{
		if (target_type == TYPE_USER)
		{
			userrec* user = (userrec*)item;
			std::string* account;
			user->GetExt("accountname", account);
			if (account)
			{
				user->Shrink("accountname");
				delete account;
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
		if ((target_type == TYPE_USER) && (extname == "accountname"))
		{	
			userrec* dest = (userrec*)target;
			/* logging them out? */
			if (extdata == "")
			{
				std::string* account;
				dest->GetExt("accountname", account);
				if (account)
				{
					dest->Shrink("accountname");
					delete account;
				}
			}
			else
			{
				// if they dont already have an accountname field, accept the remote server's
				std::string* text;
				if (!dest->GetExt("accountname", text))
				{
					text = new std::string(extdata);
					dest->Extend("accountname", text);
				}
			}
		}
	}

	virtual ~ModuleServicesAccount()
	{
		DELETE(m1);
		DELETE(m2);
		DELETE(m3);
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
	}
};


class ModuleServicesAccountFactory : public ModuleFactory
{
 public:
	ModuleServicesAccountFactory()
	{
	}
	
	~ModuleServicesAccountFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleServicesAccount(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleServicesAccountFactory;
}
