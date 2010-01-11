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

#define _CRT_SECURE_NO_DEPRECATE
#define _SCL_SECURE_NO_DEPRECATE

#include "inspircd.h"
#include "u_listmode.h"

/* $ModDesc: Provides the ability to allow channel operators to be exempt from certain modes. */
/* $ModDep: ../../include/u_listmode.h */

/** Handles channel mode +X
 */
class ExemptChanOps : public ListModeBase
{
 public:
	ExemptChanOps(Module* Creator) : ListModeBase(Creator, "exemptchanops", 'X', "End of channel exemptchanops list", 954, 953, false, "exemptchanops") { }

	virtual bool ValidateParam(User* user, Channel* chan, std::string &word)
	{
		if ((word.length() > 35) || (word.empty()))
		{
			user->WriteNumeric(955, "%s %s %s :word is too %s for exemptchanops list",user->nick.c_str(), chan->name.c_str(), word.c_str(), (word.empty() ? "short" : "long"));
			return false;
		}

		return true;
	}

	virtual bool TellListTooLong(User* user, Channel* chan, std::string &word)
	{
		user->WriteNumeric(959, "%s %s %s :Channel exemptchanops list is full", user->nick.c_str(), chan->name.c_str(), word.c_str());
		return true;
	}

	virtual void TellAlreadyOnList(User* user, Channel* chan, std::string &word)
	{
		user->WriteNumeric(957, "%s %s :The word %s is already on the exemptchanops list",user->nick.c_str(), chan->name.c_str(), word.c_str());
	}

	virtual void TellNotSet(User* user, Channel* chan, std::string &word)
	{
		user->WriteNumeric(958, "%s %s :No such exemptchanops word is set",user->nick.c_str(), chan->name.c_str());
	}
};

class ModuleExemptChanOps : public Module
{
	ExemptChanOps ec;
	std::string alwaysexempt, neverexempt;

 public:

	ModuleExemptChanOps()
		: ec(this)
	{
		if (!ServerInstance->Modes->AddMode(&ec))
			throw ModuleException("Could not add new modes!");

		ec.DoImplements(this);
		Implementation eventlist[] = { I_OnChannelDelete, I_OnChannelRestrictionApply, I_OnRehash, I_OnSyncChannel };
		ServerInstance->Modules->Attach(eventlist, this, 4);

		OnRehash(NULL);
	}

	virtual Version GetVersion()
	{
		return Version("Provides the ability to allow channel operators to be exempt from certain modes.",VF_VENDOR|VF_COMMON);
	}

	virtual void OnRehash(User* user)
	{
		ConfigReader Conf;
		alwaysexempt = Conf.ReadValue("exemptchanops", "alwaysexempt", 0);
		neverexempt = Conf.ReadValue("exemptchanops", "neverexempt", 0);
		ec.DoRehash();
	}

	virtual void OnCleanup(int target_type, void* item)
	{
		ec.DoCleanup(target_type, item);
	}

	virtual void OnSyncChannel(Channel* chan, Module* proto, void* opaque)
	{
		ec.DoSyncChannel(chan, proto, opaque);
	}

	virtual ModResult OnChannelRestrictionApply(User* user, Channel* chan, const char* restriction)
	{
		irc::spacesepstream allowstream(alwaysexempt), denystream(neverexempt);
		std::string current;

		if (chan->GetPrefixValue(user) != OP_VALUE)
			return MOD_RES_PASSTHRU; // They're not opped, so we don't exempt them
		while(denystream.GetToken(current))
			if (!strcasecmp(restriction, current.c_str())) return MOD_RES_PASSTHRU; // This mode is set to never allow exemptions in the config
		while(allowstream.GetToken(current))
			if (!strcasecmp(restriction, current.c_str())) return MOD_RES_ALLOW; // This mode is set to always allow exemptions in the config

		modelist* list = ec.extItem.get(chan);

		if (!list) return MOD_RES_PASSTHRU;
		for (modelist::iterator i = list->begin(); i != list->end(); ++i)
			if (!strcasecmp(restriction, i->mask.c_str()))
				return MOD_RES_ALLOW; //  They're opped, and the channel lets ops bypass this mode.  Allow regardless of restrictions

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleExemptChanOps)
