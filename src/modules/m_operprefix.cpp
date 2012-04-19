/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
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

class OperPrefixMode : public ModeHandler
{
	public:
		OperPrefixMode(Module* Creator, char pfx) : ModeHandler(Creator, "operprefix", 'y', PARAM_ALWAYS, MODETYPE_CHANNEL)
		{
			list = true;
			prefix = pfx;
			levelrequired = OPERPREFIX_VALUE;
			m_paramtype = TR_NICK;
		}

		unsigned int GetPrefixRank()
		{
			return OPERPREFIX_VALUE;
		}

		ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
		{
			if (IS_SERVER(source) || (source && ServerInstance->ULine(source->server)))
				return MODEACTION_ALLOW;
			else
			{
				if (source && channel)
					source->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :Only servers are permitted to change channel mode '%c'", source->nick.c_str(), channel->name.c_str(), 'y');
				return MODEACTION_DENY;
			}
		}

		bool NeedsOper() { return true; }
};

class ModuleOperPrefixMode : public Module
{
 private:
	OperPrefixMode* opm;
 public:
	ModuleOperPrefixMode() 	{
		ConfigReader Conf;
		std::string pfx = Conf.ReadValue("operprefix", "prefix", "!", 0, false);

		opm = new OperPrefixMode(this, pfx[0]);
		if ((!ServerInstance->Modes->AddMode(opm)))
			throw ModuleException("Could not add a new mode!");

		Implementation eventlist[] = { I_OnPostJoin, I_OnUserQuit, I_OnUserKick, I_OnUserPart, I_OnOper };
		ServerInstance->Modules->Attach(eventlist, this, 5);
	}

	void PushChanMode(Channel* channel, User* user)
	{
		char modeline[] = "+y";
		std::vector<std::string> modechange;
		modechange.push_back(channel->name);
		modechange.push_back(modeline);
		modechange.push_back(user->nick);
		ServerInstance->SendMode(modechange,ServerInstance->FakeClient);
	}

	void OnPostJoin(Membership* memb)
	{
		if (IS_OPER(memb->user) && !memb->user->IsModeSet('H'))
			PushChanMode(memb->chan, memb->user);
	}

	// XXX: is there a better way to do this?
	ModResult OnRawMode(User* user, Channel* chan, const char mode, const std::string &param, bool adding, int pcnt)
	{
		/* force event propagation to its ModeHandler */
		if (!IS_SERVER(user) && chan && (mode == 'y'))
			return MOD_RES_ALLOW;
		return MOD_RES_PASSTHRU;
	}

	void OnOper(User *user, const std::string&)
	{
		if (user && !user->IsModeSet('H'))
		{
			for (UCListIter v = user->chans.begin(); v != user->chans.end(); v++)
			{
				PushChanMode(*v, user);
			}
		}
	}

	~ModuleOperPrefixMode()
	{
		delete opm;
	}

	Version GetVersion()
	{
		return Version("Gives opers cmode +y which provides a staff prefix.", VF_VENDOR);
	}
};

MODULE_INIT(ModuleOperPrefixMode)
