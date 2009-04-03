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

/* $ModDesc: Allows opers to set +W to see when a user uses WHOIS on them */

/** Handle user mode +W
 */
class SeeWhois : public ModeHandler
{
 public:
	SeeWhois(InspIRCd* Instance, bool IsOpersOnly) : ModeHandler(Instance, 'W', 0, 0, false, MODETYPE_USER, IsOpersOnly) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool)
	{
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
	bool ShowWhoisFromOpers;
	SeeWhois* sw;

 public:

	ModuleShowwhois(InspIRCd* Me) : Module(Me)
	{
		ConfigReader conf(ServerInstance);
		bool OpersOnly = conf.ReadFlag("showwhois", "opersonly", "yes", 0);
		ShowWhoisFromOpers = conf.ReadFlag("showwhois", "showfromopers", "yes", 0);

		sw = new SeeWhois(ServerInstance, OpersOnly);
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
		return Version("$Id$",VF_COMMON|VF_VENDOR,API_VERSION);
	}

	virtual void OnWhois(User* source, User* dest)
	{
		if ((dest->IsModeSet('W')) && (source != dest))
		{
			if (!ShowWhoisFromOpers && IS_OPER(source))
				return;

			std::string wmsg = "*** ";
			wmsg += source->nick + " (" + source->ident + "@";

			if (dest->HasPrivPermission("users/auspex"))
			{
				wmsg += source->host;
			}
			else
			{
				wmsg += source->dhost;
			}

			wmsg += ") did a /whois on you";

			if (IS_LOCAL(dest))
			{
				dest->WriteServ("NOTICE %s :%s", dest->nick.c_str(), wmsg.c_str());
			}
			else
			{
				std::string msg = std::string("::") + dest->server + " NOTICE " + dest->nick + " :" + wmsg;
				ServerInstance->PI->PushToClient(dest, msg);
			}
		}
	}

};

MODULE_INIT(ModuleShowwhois)
