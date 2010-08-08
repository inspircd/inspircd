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
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "builtin-modes.h"

ModeChannelLimit::ModeChannelLimit() : ParamChannelModeHandler(NULL, "limit", 'l')
{
}

bool ModeChannelLimit::ResolveModeConflict(std::string &their_param, const std::string &our_param, Channel*)
{
	/* When TS is equal, the higher channel limit wins */
	return (atoi(their_param.c_str()) < atoi(our_param.c_str()));
}

bool ModeChannelLimit::ParamValidate(std::string &parameter)
{
	int limit = atoi(parameter.c_str());

	if (limit < 0)
		return false;

	parameter = ConvToStr(limit);
	return true;
}
