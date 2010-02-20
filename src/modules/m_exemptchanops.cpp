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
#include "u_listmode.h"

/* $ModDesc: Provides the ability to allow channel operators to be exempt from certain modes. */

/** Handles channel mode +X
 */
class ExemptChanOps : public ListModeBase
{
 public:
	ExemptChanOps(Module* Creator) : ListModeBase(Creator, "exemptchanops", 'X', "End of channel exemptchanops list", 954, 953, false, "exemptchanops") { fixed_letter = false; }

	bool ValidateParam(User* user, Channel* chan, std::string &word)
	{
		// TODO actually make sure there's a prop for this
		if ((word.length() > 35) || (word.empty()))
		{
			user->WriteNumeric(955, "%s %s %s :word is too %s for exemptchanops list",user->nick.c_str(), chan->name.c_str(), word.c_str(), (word.empty() ? "short" : "long"));
			return false;
		}

		return true;
	}

	bool TellListTooLong(User* user, Channel* chan, std::string &word)
	{
		user->WriteNumeric(959, "%s %s %s :Channel exemptchanops list is full", user->nick.c_str(), chan->name.c_str(), word.c_str());
		return true;
	}

	void TellAlreadyOnList(User* user, Channel* chan, std::string &word)
	{
		user->WriteNumeric(957, "%s %s :The word %s is already on the exemptchanops list",user->nick.c_str(), chan->name.c_str(), word.c_str());
	}

	void TellNotSet(User* user, Channel* chan, std::string &word)
	{
		user->WriteNumeric(958, "%s %s :No such exemptchanops word is set",user->nick.c_str(), chan->name.c_str());
	}
};

class ExemptHandler : public HandlerBase3<ModResult, User*, Channel*, const std::string&>
{
 public:
	ExemptChanOps ec;
	ExemptHandler(Module* me) : ec(me) {}
	ModResult Call(User* user, Channel* chan, const std::string& restriction)
	{
		unsigned int mypfx = chan->GetPrefixValue(user);
		std::string minmode;

		modelist* list = ec.extItem.get(chan);

		if (list)
		{
			for (modelist::iterator i = list->begin(); i != list->end(); ++i)
			{
				std::string::size_type pos = (**i).mask.find(':');
				if (pos == std::string::npos)
					continue;
				if ((**i).mask.substr(0,pos) == restriction)
					minmode = (**i).mask.substr(pos + 1);
			}
		}

		ModeHandler* mh = minmode.length() == 1 ?
			ServerInstance->Modes->FindMode(minmode[0], MODETYPE_CHANNEL) :
			ServerInstance->Modes->FindMode(minmode);
		if (mh && mypfx >= mh->GetPrefixRank())
			return MOD_RES_ALLOW;
		if (mh || minmode == "*")
			return MOD_RES_DENY;
		return ServerInstance->HandleOnCheckExemption.Call(user, chan, restriction);
	}
};

class ModuleExemptChanOps : public Module
{
	ExemptHandler eh;

 public:

	ModuleExemptChanOps() : eh(this)
	{
	}

	void init()
	{
		eh.ec.init();
		ServerInstance->Modules->AddService(eh.ec);
		Implementation eventlist[] = { I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 1);
		ServerInstance->OnCheckExemption = &eh;

		OnRehash(NULL);
	}

	~ModuleExemptChanOps()
	{
		ServerInstance->OnCheckExemption = &ServerInstance->HandleOnCheckExemption;
	}

	Version GetVersion()
	{
		return Version("Provides the ability to allow channel operators to be exempt from certain modes.",VF_VENDOR);
	}

	void OnRehash(User* user)
	{
		eh.ec.DoRehash();
	}

};

MODULE_INIT(ModuleExemptChanOps)
