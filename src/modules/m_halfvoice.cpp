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
		fixed_letter = false;
	}

	void SetPrefix(char pfx) { prefix = pfx; }

	unsigned int GetPrefixRank()
	{
		return HALFVOICE_VALUE;
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
