using namespace std;

// showwhois module by typobox43
// Modified by Craig

#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"
#include "inspircd.h"

/* $ModDesc: Allows opers to set +W to see when a user uses WHOIS on them */



class SeeWhois : public ModeHandler
{
 public:
	SeeWhois(InspIRCd* Instance) : ModeHandler(Instance, 'W', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		/* Only opers can change other users modes */
		if ((source != dest) && (!*source->oper))
			return MODEACTION_DENY;

		if (adding)
		{
			if (!channel->IsModeSet('W'))
			{
				dest->SetMode('W',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('W'))
			{
				dest->SetMode('W',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleShowwhois : public Module
{
	
	SeeWhois* sw;

 public:

	ModuleShowwhois(InspIRCd* Me) : Module::Module(Me)
	{
		
		sw = new SeeWhois(ServerInstance);
		ServerInstance->AddMode(sw, 'W');
	}

	~ModuleShowwhois()
	{
		DELETE(sw);
	}

	void Implements(char* List)
	{
		List[I_OnWhois] = 1;
	}

	virtual Version GetVersion()
	{
		return Version(1,0,0,3,VF_STATIC);
	}

	virtual void OnWhois(userrec* source, userrec* dest)
	{
		if ((dest->IsModeSet('W')) && (source != dest))
		{
			dest->WriteServ("NOTICE %s :*** %s (%s@%s) did a /whois on you.",dest->nick,source->nick,source->ident,source->host);
		}
	}

};

class ModuleShowwhoisFactory : public ModuleFactory
{
	public:
		ModuleShowwhoisFactory()
		{
		}

		~ModuleShowwhoisFactory()
		{
		}

		virtual Module* CreateModule(InspIRCd* Me)
		{
			return new ModuleShowwhois(Me);
		}

};

extern "C" void* init_module()
{
	return new ModuleShowwhoisFactory;
}
