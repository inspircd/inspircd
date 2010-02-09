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

/* $ModDesc: Provides channel mode +V, adding the - prefix
 *  which does nothing but serves as a status symbol. */

#define HALFVOICE_VALUE 1

class HalfVoiceMode : public ModeHandler
{
 public:
	HalfVoiceMode(Module* parent) : ModeHandler(parent, "halfvoice", 'V', PARAM_ALWAYS, MODETYPE_CHANNEL)
	{
		list = true;
		prefix = 0;
		levelrequired = HALFOP_VALUE;
		m_paramtype = TR_NICK;
	}

	void SetPrefix(char pfx) { prefix = pfx; }

	unsigned int GetPrefixRank()
	{
		return STATUS_VALUE;
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
			if (i->second->hasMode('V'))
			{
				if (stack)
					stack->Push(this->GetModeChar(), i->first->nick);
				else
					modestack.Push(this->GetModeChar(), i->first->nick);
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

	void RemoveMode(User* user, irc::modestacker* stack)
	{
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		return MODEACTION_ALLOW;
	}
};

class ModuleHalfVoice : public Module
{
	HalfVoiceMode mh;

 public:
	ModuleHalfVoice() : mh(this)
	{
	}

	void init()
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("halfvoice");
		std::string pfxchar = tag->getString("prefix", "-");
		mh.SetPrefix(pfxchar[0]);
		ServerInstance->Modules->AddService(mh);
	}

	Version GetVersion()
	{
		return Version("Provides a channel mode that does nothing but serve as a status symbol", VF_VENDOR);
	}
};

MODULE_INIT(ModuleHalfVoice)
