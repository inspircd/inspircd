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
	ExemptChanOps(Module* Creator) : ListModeBase(Creator, "exemptchanops", 'X', "End of channel exemptchanops list", 954, 953, false, "exemptchanops") { }

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

class ModuleExemptChanOps : public Module
{
	ExemptChanOps ec;
	std::string defaults;

 public:

	ModuleExemptChanOps() : ec(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(ec);
		Implementation eventlist[] = { I_OnChannelDelete, I_OnChannelRestrictionApply, I_OnRehash, I_OnSyncChannel };
		ServerInstance->Modules->Attach(eventlist, this, 4);

		OnRehash(NULL);
	}

	Version GetVersion()
	{
		return Version("Provides the ability to allow channel operators to be exempt from certain modes.",VF_VENDOR);
	}

	void OnRehash(User* user)
	{
		defaults = ServerInstance->Config->ConfValue("exemptchanops")->getString("defaults");
		ec.DoRehash();
	}

	void OnSyncChannel(Channel* chan, Module* proto, void* opaque)
	{
		ec.DoSyncChannel(chan, proto, opaque);
	}

	ModResult OnChannelRestrictionApply(User* user, Channel* chan, const char* restriction)
	{
		unsigned int mypfx = chan->GetPrefixValue(user);
		irc::spacesepstream defaultstream(defaults);
		char minmode = 0;
		std::string current;

		while (defaultstream.GetToken(current))
		{
			std::string::size_type pos = current.find(':');
			if (pos == std::string::npos)
				continue;
			if (current.substr(0,pos) == restriction)
				minmode = current[pos+1];
		}
		modelist* list = ec.extItem.get(chan);

		if (list)
		{
			for (modelist::iterator i = list->begin(); i != list->end(); ++i)
			{
				std::string::size_type pos = (**i).mask.find(':');
				if (pos == std::string::npos)
					continue;
				if ((**i).mask.substr(0,pos) == restriction)
					minmode = (**i).mask[pos+1];
			}
		}

		ModeHandler* mh = ServerInstance->Modes->FindMode(minmode, MODETYPE_CHANNEL);
		if (mh && mypfx >= mh->GetPrefixRank())
			return MOD_RES_ALLOW;
		if (mh || minmode == '*')
			return MOD_RES_DENY;
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleExemptChanOps)
