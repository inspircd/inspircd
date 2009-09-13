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
	InspIRCd* const MyInstance;
	const std::string type;
	const char mode;
	const int list;
	const int end;
 protected:
	bool& remove_own_privs;
	bool& remove_other_privs;
 public:
	FounderProtectBase(InspIRCd* Instance, char Mode, const std::string &mtype, int l, int e, bool &remove_own, bool &remove_others) :
		MyInstance(Instance), type(mtype), mode(Mode), list(l), end(e), remove_own_privs(remove_own), remove_other_privs(remove_others)
	{
	}

	ModePair ModeSet(User* source, User* dest, Channel* channel, const std::string &parameter)
	{
		User* x = MyInstance->FindNick(parameter);
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
		irc::modestacker modestack(MyInstance, false);
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
			MyInstance->SendMode(mode_junk, MyInstance->FakeClient);
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
	ChanFounder(InspIRCd* Instance, Module* Creator, char my_prefix, bool &depriv_self, bool &depriv_others)
		: ModeHandler(Instance, Creator, 'q', 1, 1, true, MODETYPE_CHANNEL, false, my_prefix, 0, TR_NICK),
		  FounderProtectBase(Instance, 'q', "founder", 386, 387, depriv_self, depriv_others) { }

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

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		User* theuser = ServerInstance->FindNick(parameter);

		if (!theuser)
		{
			return MODEACTION_DENY;
		}

		if ((!adding) && FounderProtectBase::CanRemoveOthers(source, channel))
		{
			return MODEACTION_ALLOW;
		}

		char isoverride=0;
		Module *Override = ServerInstance->Modules->FindFeature("Override");
		if (Override)
		{
			OVRrequest ovr(NULL,Override,source,"OTHERMODE");
			const char * tmp = ovr.Send();
			isoverride = tmp[0];
		}
		 // source is a server, or ulined, we'll let them +-q the user.
		if (!IS_LOCAL(source) ||
				((source == theuser) && (!adding) && (FounderProtectBase::remove_own_privs)) ||
				(ServerInstance->ULine(source->nick.c_str())) ||
				(ServerInstance->ULine(source->server)) ||
				(!*source->server) ||
				isoverride)
		{
			return MODEACTION_ALLOW;
		}
		else
		{
			// whoops, someones being naughty!
			source->WriteNumeric(468, "%s %s :Only servers may set channel mode +q", source->nick.c_str(), channel->name.c_str());
			parameter.clear();
			return MODEACTION_DENY;
		}
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
	ChanProtect(InspIRCd* Instance, Module* Creator, char my_prefix, bool &depriv_self, bool &depriv_others)
		: ModeHandler(Instance, Creator, 'a', 1, 1, true, MODETYPE_CHANNEL, false, my_prefix, 0, TR_NICK),
		  FounderProtectBase(Instance,'a',"protected user", 388, 389, depriv_self, depriv_others) { }

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

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		User* theuser = ServerInstance->FindNick(parameter);

		if (!theuser)
			return MODEACTION_DENY;

		if ((!adding) && FounderProtectBase::CanRemoveOthers(source, channel))
		{
			return MODEACTION_ALLOW;
		}

		char isoverride=0;
		Module *Override = ServerInstance->Modules->FindFeature("Override");
		if (Override)
		{
			OVRrequest ovr(NULL,Override,source,"OTHERMODE");
			const char * tmp = ovr.Send();
			isoverride = tmp[0];
		}
		// source has +q, is a server, or ulined, we'll let them +-a the user.
		if (!IS_LOCAL(source) ||
			((source == theuser) && (!adding) && (FounderProtectBase::remove_own_privs)) ||
			(ServerInstance->ULine(source->nick.c_str())) ||
			(ServerInstance->ULine(source->server)) ||
			(!*source->server) ||
			(channel->GetPrefixValue(source) > PROTECT_VALUE) ||
			isoverride
			)
		{
			return MODEACTION_ALLOW;
		}
		else
		{
			// bzzzt, wrong answer!
			source->WriteNumeric(482, "%s %s :You are not a channel founder", source->nick.c_str(), channel->name.c_str());
			return MODEACTION_DENY;
		}
	}

	virtual void DisplayList(User* user, Channel* channel)
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

	ModuleChanProtect(InspIRCd* Me)
		: Module(Me), FirstInGetsFounder(false), QPrefix(0), APrefix(0), DeprivSelf(false), DeprivOthers(false), booting(true), cp(NULL), cf(NULL)
	{
		/* Load config stuff */
		LoadSettings();
		booting = false;

		/* Initialise module variables */

		cp = new ChanProtect(ServerInstance, this, APrefix, DeprivSelf, DeprivOthers);
		cf = new ChanFounder(ServerInstance, this, QPrefix, DeprivSelf, DeprivOthers);

		if (!ServerInstance->Modes->AddMode(cp) || !ServerInstance->Modes->AddMode(cf))
		{
			delete cp;
			delete cf;
			throw ModuleException("Could not add new modes!");
		}

		Implementation eventlist[] = { I_OnUserKick, I_OnUserPart, I_OnUserPreJoin, I_OnAccessCheck };
		ServerInstance->Modules->Attach(eventlist, this, 4);
	}

	void LoadSettings()
	{
		ConfigReader Conf(ServerInstance);

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

	virtual ModResult OnUserPreJoin(User *user, Channel *chan, const char *cname, std::string &privs, const std::string &keygiven)
	{
		// if the user is the first user into the channel, mark them as the founder, but only if
		// the config option for it is set

		if (FirstInGetsFounder && !chan)
			privs = std::string(1, QPrefix) + "@";

		return MOD_RES_PASSTHRU;
	}

	virtual ModResult OnAccessCheck(User* source,User* dest,Channel* channel,int access_type)
	{
		// here we perform access checks, this is the important bit that actually stops kicking/deopping
		// etc of protected users. There are many types of access check, we're going to handle
		// a relatively small number of them relevent to our module using a switch statement.
		// don't allow action if:
		// (A) Theyre founder (no matter what)
		// (B) Theyre protected, unless you're founder or are protected and DeprivOthers is enabled
		// always allow the action if:
		// (A) The source is ulined

		if ((ServerInstance->ULine(source->nick.c_str())) || (ServerInstance->ULine(source->server)) || (!*source->server))
			return MOD_RES_PASSTHRU;

		if (!channel)
			return MOD_RES_PASSTHRU;

		// Can do anything to yourself if deprotectself is enabled.
		if (DeprivSelf && source == dest)
			return MOD_RES_PASSTHRU;

		Membership* smemb = channel->GetUser(source);
		Membership* dmemb = channel->GetUser(source);
		bool candepriv_founder = (DeprivOthers && smemb && smemb->hasMode('q'));
		bool candepriv_protect = smemb && (smemb->hasMode('q') || (DeprivOthers && smemb->hasMode('a')));
		bool desthas_founder = dmemb->hasMode('q');
		bool desthas_protect = dmemb->hasMode('a');

		switch (access_type)
		{
			// a user has been deopped. Do we let them? hmmm...
			case AC_DEOP:
				if (desthas_founder && !candepriv_founder)
				{
					source->WriteNumeric(484, source->nick+" "+channel->name+" :Can't deop "+dest->nick+" as they're a channel founder");
					return MOD_RES_DENY;
				}
				if (desthas_protect && !candepriv_protect)
				{
					source->WriteNumeric(484, source->nick+" "+channel->name+" :Can't deop "+dest->nick+" as they're protected (+a)");
					return MOD_RES_DENY;
				}
			break;

			// a user is being kicked. do we chop off the end of the army boot?
			case AC_KICK:
				if (desthas_founder && !candepriv_founder)
				{
					source->WriteNumeric(484, source->nick+" "+channel->name+" :Can't kick "+dest->nick+" as they're a channel founder");
					return MOD_RES_DENY;
				}
				if (desthas_protect && !candepriv_protect)
				{
					source->WriteNumeric(484, source->nick+" "+channel->name+" :Can't kick "+dest->nick+" as they're protected (+a)");
					return MOD_RES_DENY;
				}
			break;

			// a user is being dehalfopped. Yes, we do disallow -h of a +ha user
			case AC_DEHALFOP:
				if (desthas_founder && !candepriv_founder)
				{
					source->WriteNumeric(484, source->nick+" "+channel->name+" :Can't de-halfop "+dest->nick+" as they're a channel founder");
					return MOD_RES_DENY;
				}
				if (desthas_protect && !candepriv_protect)
				{
					source->WriteNumeric(484, source->nick+" "+channel->name+" :Can't de-halfop "+dest->nick+" as they're protected (+a)");
					return MOD_RES_DENY;
				}
			break;

			// same with devoice.
			case AC_DEVOICE:
				if (desthas_founder && !candepriv_founder)
				{
					source->WriteNumeric(484, source->nick+" "+channel->name+" :Can't devoice "+dest->nick+" as they're a channel founder");
					return MOD_RES_DENY;
				}
				if (desthas_protect && !candepriv_protect)
				{
					source->WriteNumeric(484, source->nick+" "+channel->name+" :Can't devoice "+dest->nick+" as they're protected (+a)");
					return MOD_RES_DENY;
				}
			break;
		}

		// we dont know what this access check is, or dont care. just carry on, nothing to see here.
		return MOD_RES_PASSTHRU;
	}

	virtual ~ModuleChanProtect()
	{
		ServerInstance->Modes->DelMode(cp);
		ServerInstance->Modes->DelMode(cf);
		delete cp;
		delete cf;
	}

	virtual Version GetVersion()
	{
		return Version("Founder and Protect modes (+qa)", VF_COMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleChanProtect)
