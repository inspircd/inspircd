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
#include "opflags.h"
#include "u_listmode.h"

/* $ModDesc: Provides the ability to adjust the prefix required for setting modes */

/** Handles channel mode +W
 */
class ModeAccess : public ListModeBase
{
 public:
	ModeAccess(Module* Creator) : ListModeBase(Creator, "modeaccess", 'W', "End of channel modeaccess list", 954, 953, false, "modeaccess") { fixed_letter = false; }

	bool ValidateParam(User* user, Channel* chan, std::string &word)
	{
		return true;
	}

	bool TellListTooLong(User* user, Channel* chan, std::string &word)
	{
		user->WriteNumeric(959, "%s %s %s :Channel modeaccess list is full", user->nick.c_str(), chan->name.c_str(), word.c_str());
		return true;
	}

	void TellAlreadyOnList(User* user, Channel* chan, std::string &word)
	{
		user->WriteNumeric(957, "%s %s :The word %s is already on the modeaccess list",user->nick.c_str(), chan->name.c_str(), word.c_str());
	}

	void TellNotSet(User* user, Channel* chan, std::string &word)
	{
		user->WriteNumeric(958, "%s %s :No such modeaccess word is set",user->nick.c_str(), chan->name.c_str());
	}
};

class ModeCheckHandler : public HandlerBase3<ModResult, User*, Channel*, irc::modechange&>
{
 public:
	ModeAccess mode;
	dynamic_reference<OpFlagProvider> permcheck;
	ModeCheckHandler(Module* parent) : mode(parent), permcheck("opflags") {}

	ModResult Call(User* user, Channel* chan, irc::modechange& mc)
	{
		ModeHandler* mh = ServerInstance->Modes->FindMode(mc.mode);
		Membership* memb = chan->GetUser(user);

		unsigned int neededrank = mh->GetLevelRequired();
		ModeHandler* neededmh = NULL;
		for(ModeIDIter id; id; id++)
		{
			ModeHandler* privmh = ServerInstance->Modes->FindMode(id);
			if (privmh && privmh->GetPrefixRank() >= neededrank)
			{
				// this mode is sufficient to allow this action
				if (!neededmh || privmh->GetPrefixRank() < neededmh->GetPrefixRank())
					neededmh = privmh;
			}
		}

		std::string neededname = neededmh ? neededmh->name : "";

		modelist* list = mode.extItem.get(chan);
		if (list)
		{
			for (modelist::iterator i = list->begin(); i != list->end(); ++i)
			{
				std::string::size_type pos = (**i).mask.find(':');
				if (pos == std::string::npos)
					continue;
				if ((**i).mask.substr(0,pos) == mh->name)
				{
					// overridden
					neededname = (**i).mask.substr(pos + 1);
					if (permcheck)
					{
						ModResult res = permcheck->PermissionCheck(memb, "mode/" + mh->name, neededname);
						if (res == MOD_RES_ALLOW)
							neededrank = 0;
						if (res == MOD_RES_DENY)
							neededrank = INT_MAX;
					}
					else
					{
						ModeHandler* privmh = neededname.length() == 1 ?
							ServerInstance->Modes->FindMode(neededname[0], MODETYPE_CHANNEL) :
							ServerInstance->Modes->FindMode(neededname);
						neededrank = privmh ? privmh->GetPrefixRank() : INT_MAX;
					}
				}
			}
		}

		if (!neededrank || (memb && memb->getRank() >= neededrank))
			return MOD_RES_ALLOW;

		if (neededname.empty())
			user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You cannot %sset the %s channel mode",
				user->nick.c_str(), chan->name.c_str(), mc.adding ? "" : "un", mh->name.c_str());
		else
			user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You must have %s access to %sset the %s channel mode",
				user->nick.c_str(), chan->name.c_str(), neededname.c_str(), mc.adding ? "" : "un", mh->name.c_str());

		return MOD_RES_DENY;
	}
};

class ModuleModeAccess : public Module
{
	ModeCheckHandler mc;
 public:

	ModuleModeAccess() : mc(this)
	{
	}

	void init()
	{
		mc.mode.init();
		ServerInstance->Modules->AddService(mc.mode);
		ServerInstance->ModeAccessCheck = &mc;

		Implementation eventlist[] = { I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 1);

		OnRehash(NULL);
	}

	~ModuleModeAccess()
	{
		ServerInstance->ModeAccessCheck = &ServerInstance->HandleModeAccessCheck;
	}

	Version GetVersion()
	{
		return Version("Provides the ability to adjust the prefix required for setting modes",VF_VENDOR);
	}

	void OnRehash(User* user)
	{
		mc.mode.DoRehash();
	}
};

MODULE_INIT(ModuleModeAccess)
