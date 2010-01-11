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
#include "channels.h"

class InspIRCd;

/** Channel mode +v
 */
class ModeChannelVoice : public ModeHandler
{
 private:
 public:
	ModeChannelVoice();
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding);
	unsigned int GetPrefixRank();
	void RemoveMode(User* user, irc::modestacker* stack = NULL);
	void RemoveMode(Channel* channel, irc::modestacker* stack = NULL);
};

