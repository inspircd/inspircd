/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
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
	SeeWhois(Module* Creator) : ModeHandler(Creator, "showwhois", 'W', PARAM_NONE, MODETYPE_USER)
	{
	}

	void SetOper(bool IsOpersOnly)
	{
		oper = IsOpersOnly;
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
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

class WhoisNoticeCmd : public Command
{
 public:
	WhoisNoticeCmd(Module* Creator) : Command(Creator,"WHOISNOTICE", 1)
	{
		flags_needed = FLAG_SERVERONLY;
	}

	void HandleFast(User* dest, User* src)
	{
		dest->WriteServ("NOTICE %s :*** %s (%s@%s) did a /whois on you",
			dest->nick.c_str(), src->nick.c_str(), src->ident.c_str(),
			dest->HasPrivPermission("users/auspex") ? src->host.c_str() : src->dhost.c_str());
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);
		User* source = ServerInstance->FindNick(parameters[1]);

		if (IS_LOCAL(dest) && source)
			HandleFast(dest, source);

		return CMD_SUCCESS;
	}
};

class ModuleShowwhois : public Module
{
	bool ShowWhoisFromOpers;
	WhoisNoticeCmd cmd;
	SeeWhois sw;

 public:

	ModuleShowwhois() : cmd(this), sw(this)
	{
	}

	void init()
	{
		ConfigTag* tag = ServerInstance->Config->GetTag("showwhois");
		sw.SetOper(tag->getBool("opersonly", true));
		ShowWhoisFromOpers = tag->getBool("showfromopers", true);

		ServerInstance->Modules->AddService(sw);
		ServerInstance->Modules->AddService(cmd);
		Implementation eventlist[] = { I_OnWhois };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	Version GetVersion()
	{
		return Version("Allows opers to set +W to see when a user uses WHOIS on them",VF_OPTCOMMON|VF_VENDOR);
	}

	void OnWhois(User* source, User* dest)
	{
		if (!dest->IsModeSet('W') || source == dest)
			return;

		if (!ShowWhoisFromOpers && (!IS_OPER(source) != !IS_OPER(dest)))
			return;

		if (IS_LOCAL(dest))
		{
			cmd.HandleFast(dest, source);
		}
		else
		{
			std::vector<std::string> params;
			params.push_back(dest->server);
			params.push_back("WHOISNOTICE");
			params.push_back(dest->uuid);
			params.push_back(source->uuid);
			ServerInstance->PI->SendEncapsulatedData(params);
		}
	}
};

MODULE_INIT(ModuleShowwhois)
