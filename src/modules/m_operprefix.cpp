/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/*
 * Originally by Chernov-Phoenix Alexey (Phoenix@RusNet) mailto:phoenix /email address separator/ pravmail.ru
 */

/* $ModDesc: Gives opers cmode +y which provides a staff prefix. */

#include "inspircd.h"

#define OPERPREFIX_VALUE 1000000

std::set<std::string>* SetupExt(User* user)
{
	std::set<std::string>* ext;
	if (!user->GetExt("m_operprefix",ext))
	{
		ext=new std::set<std::string>;
		ext->clear();
		user->Extend("m_operprefix",ext);
	}
	return ext;
}


void DelPrefixChan(User* user, Channel* channel)
{
	std::set<std::string>* chans = SetupExt(user);
	chans->erase(channel->name);
}


void AddPrefixChan(User* user, Channel* channel)
{
	std::set<std::string>* chans = SetupExt(user);
	chans->insert(channel->name);
}


class OperPrefixMode : public ModeHandler
{
	public:
		OperPrefixMode(InspIRCd* Instance, char pfx) : ModeHandler(Instance, 'y', 1, 1, true, MODETYPE_CHANNEL, false, pfx, pfx, TR_NICK) { }

		unsigned int GetPrefixRank()
		{
			return OPERPREFIX_VALUE;
		}

		ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool servermode)
		{
			if (servermode || (source && ServerInstance->ULine(source->server)))
				return MODEACTION_ALLOW;
			else
			{
				if (source && channel)
					source->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :Only servers are permitted to change channel mode '%c'", source->nick.c_str(), channel->name.c_str(), 'y');
				return MODEACTION_DENY;
			}
		}

		ModePair ModeSet(User* source, User* dest, Channel* channel, const std::string &parameter)
		{
			User* x = ServerInstance->FindNick(parameter);
			if (x)
			{
				if (!channel->HasUser(x))
				{
					return std::make_pair(false, parameter);
				}
				else
				{
					std::set<std::string>* ext;
					if (x->GetExt("m_operprefix",ext))
					{
						if (ext->find(channel->name)!=ext->end())
						{
							return std::make_pair(true, x->nick);
						}
						else
							return std::make_pair(false, parameter);
					}
					else
					{
						return std::make_pair(false, parameter);
					}
				}
			}
			return std::make_pair(false, parameter);
		}

		bool NeedsOper() { return true; }
};

class ModuleOperPrefixMode : public Module
{
 private:
	OperPrefixMode* opm;
 public:
	ModuleOperPrefixMode(InspIRCd* Me) : Module(Me)
	{
		ConfigReader Conf(ServerInstance);
		std::string pfx = Conf.ReadValue("operprefix", "prefix", "!", 0, false);

		opm = new OperPrefixMode(ServerInstance, pfx[0]);
		if ((!ServerInstance->Modes->AddMode(opm)))
			throw ModuleException("Could not add a new mode!");

		Implementation eventlist[] = { I_OnPostJoin, I_OnCleanup, I_OnUserQuit, I_OnUserKick, I_OnUserPart, I_OnOper };
		ServerInstance->Modules->Attach(eventlist, this, 6);
	}

	void PushChanMode(Channel* channel, User* user, bool negate = false)
	{
		if (negate)
			DelPrefixChan(user, channel);
		else
			AddPrefixChan(user, channel);
		char modeline[] = "+y";
		if (negate)
			modeline[0] = '-';
		std::vector<std::string> modechange;
		modechange.push_back(channel->name);
		modechange.push_back(modeline);
		modechange.push_back(user->nick);
		ServerInstance->SendMode(modechange,this->ServerInstance->FakeClient);
	}

	virtual void OnPostJoin(User *user, Channel *channel)
	{
		if (user && IS_OPER(user))
		{
			if (user->IsModeSet('H'))
			{
				/* we respect your wish to be invisible */
				return;
			}
			PushChanMode(channel, user);
		}
	}

	// XXX: is there a better way to do this?
	virtual int OnRawMode(User* user, Channel* chan, const char mode, const std::string &param, bool adding, int pcnt, bool servermode)
	{
		/* force event propagation to its ModeHandler */
		if (!servermode && chan && (mode == 'y'))
			return ACR_ALLOW;
		return 0;
	}

	virtual void OnOper(User *user, const std::string&)
	{
		if (user && !user->IsModeSet('H'))
		{
			for (UCListIter v = user->chans.begin(); v != user->chans.end(); v++)
			{
				PushChanMode(v->first, user);
			}
		}
	}

	virtual ~ModuleOperPrefixMode()
	{
		ServerInstance->Modes->DelMode(opm);
		delete opm;
	}

	void CleanUser(User* user, bool quitting)
	{

		std::set<std::string>* ext;
		if (user->GetExt("m_operprefix",ext))
		{
			// Don't want to announce -mode when they're quitting anyway..
			if (!quitting)
			{
				for (UCListIter v = user->chans.begin(); v != user->chans.end(); v++)
				{
					ModePair ms = opm->ModeSet(NULL, NULL , v->first, user->nick);
					if (ms.first)
					{
						PushChanMode(v->first, user, true);
					}
				}
			}
			ext->clear();
			delete ext;
			user->Shrink("m_operprefix");
		}
	}

	virtual void OnCleanup(int target_type, void* item)
	{
		if (target_type == TYPE_USER)
		{
			User* user = (User*)item;
			CleanUser(user, false);
		}
	}

	virtual void OnUserQuit(User* user, const std::string &reason, const std::string &oper_message)
	{
		CleanUser(user,true);
	}

	virtual void OnUserKick(User* source, User* user, Channel* chan, const std::string &reason, bool &silent)
	{
		DelPrefixChan(user, chan);
	}

	virtual void OnUserPart(User* user, Channel* channel, std::string &partreason, bool &silent)
	{
		DelPrefixChan(user, channel);
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleOperPrefixMode)
