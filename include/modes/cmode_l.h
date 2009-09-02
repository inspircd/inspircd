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

#include "mode.h"

class InspIRCd;

/** Channel mode +l
 */
class ModeChannelLimit : public ModeHandler
{
 public:
	ModeChannelLimit(InspIRCd* Instance);
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool servermode);
	ModePair ModeSet(User* source, User* dest, Channel* channel, const std::string &parameter);
	bool CheckTimeStamp(std::string &their_param, const std::string &our_param, Channel* channel);
};
