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

class ModeChannelHalfOp : public PrefixModeHandler
{
 public:
	ModeChannelHalfOp(Module* parent);
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding);
	unsigned int GetPrefixRank();

	ModResult AccessCheck(User* src, Channel*, std::string& value, bool adding)
	{
		if (!adding && src->nick == value)
			return MOD_RES_ALLOW;
		return MOD_RES_PASSTHRU;
	}
};

ModeChannelHalfOp::ModeChannelHalfOp(Module* parent) : PrefixModeHandler(parent, "halfop", 'h')
{
	prefix = '%';
	levelrequired = OP_VALUE;
	fixed_letter = false;
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
	ModuleHalfop() : mh(this) {}

	void init()
	{
		ServerInstance->Modules->AddService(mh);
	}

	Version GetVersion()
	{
		return Version("Channel half-operator mode provider", VF_VENDOR);
	}
};

MODULE_INIT(ModuleHalfop)
