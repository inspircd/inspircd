/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *		       E-mail:
 *		<brain@chatspike.net>
 *	   	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include <stdio.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include <string>
#include "helperfuncs.h"
#include "hashcomp.h"

/* $ModDesc: Povides support for ircu-style services accounts, including chmode +R, etc. */

class ModuleServicesAccount : public Module
{
	Server *Srv; 

 public:
	ModuleServicesAccount(Server* Me) : Module::Module(Me)
	{
		Srv = Me;

		Srv->AddExtendedMode('R',MT_CHANNEL,false,0,0);
		Srv->AddExtendedMode('R',MT_CLIENT,false,0,0);
		Srv->AddExtendedMode('M',MT_CHANNEL,false,0,0);
	}

	virtual void On005Numeric(std::string &output)
	{
		InsertMode(output, "RM", 4);
	}

	/* <- :twisted.oscnet.org 330 w00t2 w00t2 w00t :is logged in as */
	virtual void OnWhois(userrec* source, userrec* dest)
	{
		char *account = dest->GetExt("accountname");

		if (account)
		{
			std::string* accountn = (std::string*)account;
			WriteServ(source->fd, "330 %s %s %s :is logged in as", source->nick, dest->nick, accountn->c_str());
		}
	}

	void Implements(char* List)
	{
		List[I_OnWhois] = List[I_OnUserPreMessage] = List[I_OnExtendedMode] = List[I_On005Numeric] = List[I_OnUserPreNotice] = List[I_OnUserPreJoin] = 1;
		List[I_OnSyncUserMetaData] = List[I_OnUserQuit] = List[I_OnCleanup] = List[I_OnDecodeMetaData] = 1;
	}

	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		if (modechar == 'R')
		{
			return 1;
		}
		else if (modechar == 'M')
		{
			if (type == MT_CHANNEL)
			{
				return 1;
			}
		}

		return 0;
	}

	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		char *account = user->GetExt("accountname");
		
		if (target_type == TYPE_CHANNEL)
		{
			chanrec* c = (chanrec*)dest;
			
			if ((c->IsModeSet('M')) && (!account))
			{
				if ((Srv->IsUlined(user->nick)) || (Srv->IsUlined(user->server)) || (!strcmp(user->server,"")))
				{
					// user is ulined, can speak regardless
					return 0;
				}

				// user messaging a +M channel and is not registered
				Srv->SendServ(user->fd,"477 "+std::string(user->nick)+" "+std::string(c->name)+" :You need to be identified to a registered account to message this channel");
				return 1;
			}
		}
		if (target_type == TYPE_USER)
		{
			userrec* u = (userrec*)dest;
			
			if ((strchr(u->modes,'R')) && (!account))
			{
				if ((Srv->IsUlined(user->nick)) || (Srv->IsUlined(user->server)))
				{
					// user is ulined, can speak regardless
					return 0;
				}

				// user messaging a +R user and is not registered
				Srv->SendServ(user->fd,"477 "+std::string(user->nick)+" "+std::string(u->nick)+" :You need to be identified to a registered account to message this user");
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
		char *account = user->GetExt("accountname");
		
		if (chan)
		{
			if (chan->IsModeSet('R'))
			{
				if (!account)
				{
					if ((Srv->IsUlined(user->nick)) || (Srv->IsUlined(user->server)))
					{
						// user is ulined, won't be stopped from joining
						return 0;
					}
					// joining a +R channel and not identified
					Srv->SendServ(user->fd,"477 "+std::string(user->nick)+" "+std::string(chan->name)+" :You need to be identified to a registered account to join this channel");
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
			char* field = user->GetExt("accountname");
			if (field)
			{
				// get our extdata out with a cast
				std::string* account = (std::string*)field;
				// call this function in the linking module, let it format the data how it
				// sees fit, and send it on its way. We dont need or want to know how.
				proto->ProtoSendMetaData(opaque,TYPE_USER,user,extname,*account);
			}
		}
	}

	// when a user quits, tidy up their metadata
	virtual void OnUserQuit(userrec* user, const std::string &message)
	{
		char* field = user->GetExt("accountname");
		if (field)
		{
			std::string* account = (std::string*)field;
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
			char* field = user->GetExt("accountname");
			if (field)
			{
				std::string* account = (std::string*)field;
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
				char* field = dest->GetExt("accountname");
				if (field)
				{
					std::string* account = (std::string*)field;
					dest->Shrink("accountname");
					delete account;
				}
			}
			else
			{
				// if they dont already have an accountname field, accept the remote server's
				if (!dest->GetExt("accountname"))
				{
					std::string* text = new std::string(extdata);
					dest->Extend("accountname",(char*)text);
				}
			}
		}
	}

	virtual ~ModuleServicesAccount()
	{
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
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleServicesAccount(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleServicesAccountFactory;
}
