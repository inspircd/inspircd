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
#include "m_override.h"

/* $ModDesc: Provides channel modes +a and +q */

#define PROTECT_VALUE 40000
#define FOUNDER_VALUE 50000

/** Handles basic operation of +qa channel modes
 */
class FounderProtectBase
{
 private:
	const std::string type;
	const char mode;
	const int list;
	const int end;
 protected:
	bool& remove_own_privs;
	bool& remove_other_privs;
 public:
	FounderProtectBase(char Mode, const std::string &mtype, int l, int e, bool &remove_own, bool &remove_others) :
		type(mtype), mode(Mode), list(l), end(e), remove_own_privs(remove_own), remove_other_privs(remove_others)
	{
	}

	ModePair ModeSet(User* source, User* dest, Channel* channel, const std::string &parameter)
	{
		User* x = ServerInstance->FindNick(parameter);
		if (x)
		{
			Membership* memb = channel->GetUser(x);
			if (!memb)
			{
				return std::make_pair(false, parameter);
			}
			else
			{
				if (memb->hasMode(mode))
				{
					return std::make_pair(true, x->nick);
				}
				else
				{
					return std::make_pair(false, parameter);
				}
			}
		}
		return std::make_pair(false, parameter);
	}

	void RemoveMode(Channel* channel, irc::modestacker* stack)
	{
		const UserMembList* cl = channel->GetUsers();
		std::vector<std::string> mode_junk;
		mode_junk.push_back(channel->name);
		irc::modestacker modestack(false);
		std::deque<std::string> stackresult;

		for (UserMembCIter i = cl->begin(); i != cl->end(); i++)
		{
			if (i->second->hasMode(mode))
			{
				if (stack)
					stack->Push(mode, i->first->nick);
				else
					modestack.Push(mode, i->first->nick);
			}
		}

		if (stack)
			return;

		while (modestack.GetStackedLine(stackresult))
		{
			mode_junk.insert(mode_junk.end(), stackresult.begin(), stackresult.end());
			ServerInstance->SendMode(mode_junk, ServerInstance->FakeClient);
			mode_junk.erase(mode_junk.begin() + 1, mode_junk.end());
		}
	}

	void DisplayList(User* user, Channel* channel)
	{
		const UserMembList* cl = channel->GetUsers();
		for (UserMembCIter i = cl->begin(); i != cl->end(); ++i)
		{
			if (i->second->hasMode(mode))
			{
				user->WriteServ("%d %s %s %s", list, user->nick.c_str(), channel->name.c_str(), i->first->nick.c_str());
			}
		}
		user->WriteServ("%d %s %s :End of channel %s list", end, user->nick.c_str(), channel->name.c_str(), type.c_str());
	}

	bool CanRemoveOthers(User* u1, Channel* c)
	{
		Membership* m1 = c->GetUser(u1);
		return (remove_other_privs && m1 && m1->hasMode(mode));
	}
};

/** Abstraction of FounderProtectBase for channel mode +q
 */
class ChanFounder : public ModeHandler, public FounderProtectBase
{
 public:
	ChanFounder(Module* Creator, char my_prefix, bool &depriv_self, bool &depriv_others)
		: ModeHandler(Creator, 'q', PARAM_ALWAYS, MODETYPE_CHANNEL),
		  FounderProtectBase('q', "founder", 386, 387, depriv_self, depriv_others)
	{
		ModeHandler::list = true;
		prefix = my_prefix;
		levelrequired = FOUNDER_VALUE;
		m_paramtype = TR_NICK;
	}

	unsigned int GetPrefixRank()
	{
		return FOUNDER_VALUE;
	}

	ModePair ModeSet(User* source, User* dest, Channel* channel, const std::string &parameter)
	{
		return FounderProtectBase::ModeSet(source, dest, channel, parameter);
	}

	void RemoveMode(Channel* channel, irc::modestacker* stack)
	{
		FounderProtectBase::RemoveMode(channel, stack);
	}

	void RemoveMode(User* user, irc::modestacker* stack)
	{
	}
	
	ModResult AccessCheck(User* source, Channel* channel, std::string &parameter, bool adding)
	{
		User* theuser = ServerInstance->FindNick(parameter);
		// remove own privs?
		if (source == theuser && !adding && FounderProtectBase::remove_own_privs)
			return MOD_RES_ALLOW;

		if (!adding && FounderProtectBase::CanRemoveOthers(source, channel))
		{
			return MOD_RES_PASSTHRU;
		}
		else
		{
			source->WriteNumeric(468, "%s %s :Only servers may set channel mode +q", source->nick.c_str(), channel->name.c_str());
			return MOD_RES_DENY;
		}
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		User* theuser = ServerInstance->FindNick(parameter);

		if (!theuser)
			return MODEACTION_DENY;
		return MODEACTION_ALLOW;
	}

	void DisplayList(User* user, Channel* channel)
	{
		FounderProtectBase::DisplayList(user,channel);
	}
};

/** Abstraction of FounderProtectBase for channel mode +a
 */
class ChanProtect : public ModeHandler, public FounderProtectBase
{
 public:
	ChanProtect(Module* Creator, char my_prefix, bool &depriv_self, bool &depriv_others)
		: ModeHandler(Creator, 'a', PARAM_ALWAYS, MODETYPE_CHANNEL),
		  FounderProtectBase('a',"protected user", 388, 389, depriv_self, depriv_others)
	{
		ModeHandler::list = true;
		prefix = my_prefix;
		levelrequired = PROTECT_VALUE;
		m_paramtype = TR_NICK;
	}

	unsigned int GetPrefixRank()
	{
		return PROTECT_VALUE;
	}

	ModePair ModeSet(User* source, User* dest, Channel* channel, const std::string &parameter)
	{
		return FounderProtectBase::ModeSet(source, dest, channel, parameter);
	}

	void RemoveMode(Channel* channel, irc::modestacker* stack)
	{
		FounderProtectBase::RemoveMode(channel, stack);
	}

	void RemoveMode(User* user, irc::modestacker* stack)
	{
	}

	ModResult AccessCheck(User* source, Channel* channel, std::string &parameter, bool adding)
	{
		User* theuser = ServerInstance->FindNick(parameter);
		// source has +q
		if (channel->GetPrefixValue(source) > PROTECT_VALUE)
			return MOD_RES_ALLOW;

		// removing own privs?
		if (source == theuser && !adding && FounderProtectBase::remove_own_privs)
			return MOD_RES_ALLOW;

		if (!adding && FounderProtectBase::CanRemoveOthers(source, channel))
		{
			return MOD_RES_PASSTHRU;
		}
		else
		{
			source->WriteNumeric(482, "%s %s :You are not a channel founder", source->nick.c_str(), channel->name.c_str());
			return MOD_RES_DENY;
		}
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		User* theuser = ServerInstance->FindNick(parameter);

		if (!theuser)
			return MODEACTION_DENY;

		return MODEACTION_ALLOW;
	}

	void DisplayList(User* user, Channel* channel)
	{
		FounderProtectBase::DisplayList(user, channel);
	}

};

class ModuleChanProtect : public Module
{

	bool FirstInGetsFounder;
	char QPrefix;
	char APrefix;
	bool DeprivSelf;
	bool DeprivOthers;
	bool booting;
	ChanProtect* cp;
	ChanFounder* cf;

 public:

	ModuleChanProtect()
		: FirstInGetsFounder(false), QPrefix(0), APrefix(0), DeprivSelf(false), DeprivOthers(false), booting(true), cp(NULL), cf(NULL)
	{
		/* Load config stuff */
		LoadSettings();
		booting = false;

		/* Initialise module variables */

		cp = new ChanProtect(this, APrefix, DeprivSelf, DeprivOthers);
		cf = new ChanFounder(this, QPrefix, DeprivSelf, DeprivOthers);

		if (!ServerInstance->Modes->AddMode(cp) || !ServerInstance->Modes->AddMode(cf))
		{
			delete cp;
			delete cf;
			throw ModuleException("Could not add new modes!");
		}

		Implementation eventlist[] = { I_OnUserKick, I_OnUserPart, I_OnUserPreJoin };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	void LoadSettings()
	{
		ConfigReader Conf;

		FirstInGetsFounder = Conf.ReadFlag("chanprotect", "noservices", 0);

		std::string qpre = Conf.ReadValue("chanprotect", "qprefix", 0);
		QPrefix = qpre.empty() ? 0 : qpre[0];

		std::string apre = Conf.ReadValue("chanprotect", "aprefix", 0);
		APrefix = apre.empty() ? 0 : apre[0];

		if ((APrefix && QPrefix) && APrefix == QPrefix)
			throw ModuleException("What the smeg, why are both your +q and +a prefixes the same character?");

		if (cp && ServerInstance->Modes->FindPrefix(APrefix) == cp)
			throw ModuleException("Looks like the +a prefix you picked for m_chanprotect is already in use. Pick another.");

		if (cf && ServerInstance->Modes->FindPrefix(QPrefix) == cf)
			throw ModuleException("Looks like the +q prefix you picked for m_chanprotect is already in use. Pick another.");

		DeprivSelf = Conf.ReadFlag("chanprotect","deprotectself", "yes", 0);
		DeprivOthers = Conf.ReadFlag("chanprotect","deprotectothers", "yes", 0);
	}

	ModResult OnUserPreJoin(User *user, Channel *chan, const char *cname, std::string &privs, const std::string &keygiven)
	{
		// if the user is the first user into the channel, mark them as the founder, but only if
		// the config option for it is set

		if (FirstInGetsFounder && !chan)
			privs += 'q';

		return MOD_RES_PASSTHRU;
	}

	~ModuleChanProtect()
	{
		ServerInstance->Modes->DelMode(cp);
		ServerInstance->Modes->DelMode(cf);
		delete cp;
		delete cf;
	}

	Version GetVersion()
	{
		return Version("Founder and Protect modes (+qa)", VF_COMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleChanProtect)
