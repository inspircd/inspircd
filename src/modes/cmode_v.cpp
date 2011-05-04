
/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "configreader.h"
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "modules.h"
#include "builtin-modes.h"

ModeChannelVoice::ModeChannelVoice() : PrefixModeHandler(NULL, "voice", 'v')
{
	prefix = '+';
}

unsigned int ModeChannelVoice::GetPrefixRank()
{
	return VOICE_VALUE;
}

ModeAction ModeChannelVoice::OnModeChange(User* source, User*, Channel* channel, std::string &parameter, bool adding)
{
	return MODEACTION_ALLOW;
}
