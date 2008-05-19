/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Allows opers to set +W to see when a user uses WHOIS on them */

/** Handle user mode +W
 */
class SeeWhois : public ModeHandler
{
 public:
	SeeWhois(InspIRCd* Instance) : ModeHandler(Instance, 'W', 0, 0, false, MODETYPE_USER, true) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool)
	{
		/* Only opers can change other users modes */
		if (source != dest)
			return MODEACTION_DENY;

		if (adding)
		{
			if (!dest->IsModeSet('W'))
			{
				dest->SetMode('W',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('W'))
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

	ModuleShowwhois(InspIRCd* Me) : Module(Me)
	{
		
		sw = new SeeWhois(ServerInstance);
		if (!ServerInstance->Modes->AddMode(sw))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnWhois };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}

	~ModuleShowwhois()
	{
		ServerInstance->Modes->DelMode(sw);
		delete sw;
	}


	virtual Version GetVersion()
	{
		return Version(1,2,0,3,VF_COMMON|VF_VENDOR,API_VERSION);
	}

	virtual void OnWhois(User* source, User* dest)
	{
		if ((dest->IsModeSet('W')) && (source != dest))
		{
			if (IS_LOCAL(dest))
			{
				dest->WriteServ("NOTICE %s :*** %s (%s@%s) did a /whois on you.", dest->nick.c_str(), source->nick.c_str(), source->ident.c_str(), source->host.c_str());
			}
			else
			{
				std::string msg = std::string(":") + dest->server + " NOTICE " + dest->nick + " :*** " + source->nick + " (" + source->ident + "@" + source->host.c_str() + ") did a /whois on you.";
				ServerInstance->PI->PushToClient(dest, msg);
			}
		}
	}

};

MODULE_INIT(ModuleShowwhois)
