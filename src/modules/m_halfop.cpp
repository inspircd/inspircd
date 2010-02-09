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

class ModeChannelHalfOp : public ModeHandler
{
 public:
	ModeChannelHalfOp(Module* parent);
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding);
	unsigned int GetPrefixRank();
};

ModeChannelHalfOp::ModeChannelHalfOp(Module* parent) : ModeHandler(parent, "halfop", 'h', PARAM_ALWAYS, MODETYPE_CHANNEL)
{
	list = true;
	prefix = '%';
	levelrequired = OP_VALUE;
	m_paramtype = TR_NICK;
}

unsigned int ModeChannelHalfOp::GetPrefixRank()
{
	return HALFOP_VALUE;
}

ModeAction ModeChannelHalfOp::OnModeChange(User* source, User*, Channel* channel, std::string &parameter, bool adding)
{
	return MODEACTION_ALLOW;
}

class ModuleHalfop : public Module
{
	ModeChannelHalfOp mh;
 public:
	ModuleHalfop() : mh(this)
	{
		if (!ServerInstance->Modes->AddMode(&mh))
			throw ModuleException("Could not add new modes!");
	}

	~ModuleHalfop()
	{
		ServerInstance->Modes->DelMode(&mh);
	}

	Version GetVersion()
	{
		return Version("Channel half-operator mode provider", VF_VENDOR);
	}
};

MODULE_INIT(ModuleHalfop)
