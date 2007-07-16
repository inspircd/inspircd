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

/* $ModDesc: Allows opers to set +W to see when a user uses WHOIS on them */

/** Handle user mode +W
 */
class SeeWhois : public ModeHandler
{
 public:
	SeeWhois(InspIRCd* Instance) : ModeHandler(Instance, 'W', 0, 0, false, MODETYPE_USER, true) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
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
		if (!ServerInstance->AddMode(sw, 'W'))
			throw ModuleException("Could not add new modes!");
	}

	~ModuleShowwhois()
	{
		ServerInstance->Modes->DelMode(sw);
		DELETE(sw);
	}

	void Implements(char* List)
	{
		List[I_OnWhois] = 1;
	}

	virtual Version GetVersion()
	{
		return Version(1,1,0,3,VF_COMMON|VF_VENDOR,API_VERSION);
	}

	virtual void OnWhois(userrec* source, userrec* dest)
	{
		if ((dest->IsModeSet('W')) && (source != dest))
		{
			if (IS_LOCAL(dest))
			{
				dest->WriteServ("NOTICE %s :*** %s (%s@%s) did a /whois on you.",dest->nick,source->nick,source->ident,source->host);
			}
			else
			{
				std::deque<std::string> params;
				params.push_back(dest->nick);
				std::string msg = ":";
				msg = msg + dest->server + " NOTICE " + dest->nick + " :*** " + source->nick + " (" + source->ident + "@" + source->host + ") did a /whois on you.";
				params.push_back(msg);
				Event ev((char *) &params, NULL, "send_push");
				ev.Send(ServerInstance);
			}
		}
	}

};

MODULE_INIT(ModuleShowwhois)
