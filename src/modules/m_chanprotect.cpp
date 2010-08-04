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

/* $ModDesc: Provides channel modes +a and +q */

#define PROTECT_VALUE 40000
#define FOUNDER_VALUE 50000

/** Abstraction of FounderProtectBase for channel mode +q
 */
class ChanFounder : public ModeHandler
{
 public:
	ChanFounder(Module* Creator)
		: ModeHandler(Creator, "founder", 'q', PARAM_ALWAYS, MODETYPE_CHANNEL)
	{
		ModeHandler::list = true;
		levelrequired = FOUNDER_VALUE;
		m_paramtype = TR_NICK;
		fixed_letter = false;
	}

	void setPrefix(int pfx)
	{
		prefix = pfx;
	}

	void setLevel(bool selfmanip)
	{
		levelrequired = selfmanip ? FOUNDER_VALUE : FOUNDER_VALUE + 1;
	}

	unsigned int GetPrefixRank()
	{
		return FOUNDER_VALUE;
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		return MODEACTION_ALLOW;
	}

	void DisplayList(User* user, Channel* channel)
	{
		const UserMembList* cl = channel->GetUsers();
		for (UserMembCIter i = cl->begin(); i != cl->end(); ++i)
		{
			if (i->second->hasMode(mode))
			{
				user->WriteServ("386 %s %s %s", user->nick.c_str(), channel->name.c_str(), i->first->nick.c_str());
			}
		}
		user->WriteServ("387 %s %s :End of channel founder list", user->nick.c_str(), channel->name.c_str());
	}
};

/** Abstraction of FounderProtectBase for channel mode +a
 */
class ChanProtect : public ModeHandler
{
 public:
	ChanProtect(Module* Creator)
		: ModeHandler(Creator, "admin", 'a', PARAM_ALWAYS, MODETYPE_CHANNEL)
	{
		ModeHandler::list = true;
		levelrequired = PROTECT_VALUE;
		m_paramtype = TR_NICK;
		fixed_letter = false;
	}

	void setPrefix(int pfx)
	{
		prefix = pfx;
	}

	unsigned int GetPrefixRank()
	{
		return PROTECT_VALUE;
	}

	void setLevel(bool selfmanip)
	{
		levelrequired = selfmanip ? PROTECT_VALUE : PROTECT_VALUE + 1;
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		return MODEACTION_ALLOW;
	}

	void DisplayList(User* user, Channel* channel)
	{
		const UserMembList* cl = channel->GetUsers();
		for (UserMembCIter i = cl->begin(); i != cl->end(); ++i)
		{
			if (i->second->hasMode(mode))
			{
				user->WriteServ("388 %s %s %s", user->nick.c_str(), channel->name.c_str(), i->first->nick.c_str());
			}
		}
		user->WriteServ("389 %s %s :End of channel protected user list", user->nick.c_str(), channel->name.c_str());
	}
};

class ModuleChanProtect : public Module
{
	ChanProtect cp;
	ChanFounder cf;
	bool FirstInGetsFounder;
 public:
	ModuleChanProtect() : cp(this), cf(this)
	{
	}

	void init()
	{
		/* Load config stuff */
		OnRehash(NULL);
		ConfigTag* tag = ServerInstance->Config->ConfValue("chanprotect");

		std::string qpre = tag->getString("qprefix");
		std::string apre = tag->getString("aprefix");
		cf.setPrefix(qpre.empty() ? 0 : qpre[0]);
		cp.setPrefix(apre.empty() ? 0 : apre[0]);

		ServerInstance->Modules->AddService(cf);
		ServerInstance->Modules->AddService(cp);

		Implementation eventlist[] = { I_OnCheckJoin, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	void OnRehash(User*)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("chanprotect");
		cp.setLevel(tag->getBool("grantadmin", false));
		cf.setLevel(tag->getBool("grantfounder", true));
		FirstInGetsFounder = tag->getBool("noservices");
	}

	void OnCheckJoin(ChannelPermissionData& join)
	{
		// if the user is the first user into the channel, mark them as the founder, but only if
		// the config option for it is set

		if (FirstInGetsFounder && !join.chan)
			join.privs += cf.GetModeChar();
	}

	Version GetVersion()
	{
		return Version("Founder and Protect modes (+qa)", VF_VENDOR);
	}
};

MODULE_INIT(ModuleChanProtect)
