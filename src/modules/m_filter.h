/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

class FilterResult : public classbase
{
 public:
	std::string reason;
	std::string action;

	FilterResult(const std::string &rea, const std::string &act) : reason(rea), action(act)
	{
	}

	FilterResult()
	{
	}

	virtual ~FilterResult()
	{
	}
};

class FilterBase : public Module
{ 
 public:
	FilterBase(InspIRCd* Me)
		: Module::Module(Me)
	{
	}
	
	virtual ~FilterBase()
	{
	}

	virtual void Implements(char* List)
	{
		List[I_OnUserPreMessage] = List[I_OnUserPreNotice] = List[I_OnRehash] = 1;
	}

	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		return OnUserPreNotice(user,dest,target_type,text,status);
	}

	/* This must be implemented by the module which uses the header */
	virtual FilterResult* FilterMatch(const std::string &text) = 0;

	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		FilterResult* f = this->FilterMatch(text);
		if (f)
		{
			std::string target = "";
			if (target_type == TYPE_USER)
			{
				userrec* t = (userrec*)dest;
				target = std::string(t->nick);
			}
			else if (target_type == TYPE_CHANNEL)
			{
				chanrec* t = (chanrec*)dest;
				target = std::string(t->name);
			}
			if (f->action == "block")
      			{	
				ServerInstance->WriteOpers(std::string("FILTER: ")+user->nick+" had their notice filtered, target was "+target+": "+f->reason);
				user->WriteServ("NOTICE "+std::string(user->nick)+" :Your notice has been filtered and opers notified: "+f->reason);
			}
			ServerInstance->Log(DEFAULT,"FILTER: "+std::string(user->nick)+std::string(" had their notice filtered, target was ")+target+": "+f->reason+" Action: "+f->action);

			if (f->action == "kill")
			{
				userrec::QuitUser(ServerInstance,user,f->reason);
			}
			return 1;
		}
		return 0;
	}

	virtual void OnRehash(const std::string &parameter)
	{
	}
	
	virtual Version GetVersion()
	{
		// This is version 2 because version 1.x is the unreleased unrealircd module
		return Version(1,1,0,2,VF_VENDOR,API_VERSION);
	}
};
