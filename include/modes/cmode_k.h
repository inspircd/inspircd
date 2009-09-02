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

/** Channel mode +k
 */
class ModeChannelKey : public ModeHandler
{
 public:
	ModeChannelKey(InspIRCd* Instance);
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding);
	ModePair ModeSet(User* source, User* dest, Channel* channel, const std::string &parameter);
	void RemoveMode(Channel* channel, irc::modestacker* stack = NULL);
	void RemoveMode(User* user, irc::modestacker* stack = NULL);
};
