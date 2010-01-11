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

#include "mode.h"

/** Channel mode +l
 */
class ModeChannelLimit : public ParamChannelModeHandler
{
 public:
	ModeChannelLimit();
	bool ParamValidate(std::string& parameter);
	bool ResolveModeConflict(std::string &their_param, const std::string &our_param, Channel* channel);
};
